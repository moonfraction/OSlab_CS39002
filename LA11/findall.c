#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define MAX_LOGIN 256
#define PATH_MAX 4096
// UID->login mapping.
typedef struct {
    unsigned int uid;
    char login[MAX_LOGIN];
} UidMap;

UidMap *uidMapArr = NULL;
size_t uidMapCount = 0;

int serialNum = 0;


void uidmapping() {
    FILE *fp = fopen("/etc/passwd", "r");
    if (!fp) {
        perror("Error opening /etc/passwd");
        exit(EXIT_FAILURE);
    }
    
    char line[1024];
    // Initial capacity for the UID mapping array.
    size_t capacity = 64; // can expand on demand
    uidMapArr = malloc(capacity * sizeof(UidMap));
    if (!uidMapArr) {
        perror("Memory allocation failed");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = '\0';
        
        // Tokenize the line: fields delimited by ':'
        char *login = strtok(line, ":");
        if (!login)
            continue;
        
        char *dummy = strtok(NULL, ":"); // Skip password.
        if (!dummy)
            continue;
        
        char *uidStr = strtok(NULL, ":"); // Get UID string.
        if (!uidStr)
            continue;
        unsigned int uid = (unsigned int) atoi(uidStr);
        
        // Expand array if needed.
        if (uidMapCount == capacity) {
            capacity *= 2;
            uidMapArr = realloc(uidMapArr, capacity * sizeof(UidMap));
            if (!uidMapArr) {
                perror("Memory allocation failed");
                fclose(fp);
                exit(EXIT_FAILURE);
            }
        }
        
        // Store in map
        uidMapArr[uidMapCount].uid = uid;
        strncpy(uidMapArr[uidMapCount].login, login, MAX_LOGIN-1);
        uidMapArr[uidMapCount].login[MAX_LOGIN-1] = '\0';
        uidMapCount++;
    }
    fclose(fp);
}


const char *getLoginByUID(unsigned int uid) {
    for (size_t i = 0; i < uidMapCount; i++) {
        if (uidMapArr[i].uid == uid)
            return uidMapArr[i].login;
    }
    return "UNKNOWN";
}

int filename_has_ext(const char *filename, const char *ext) {
    if (!filename || !ext)
        return 0;
        
    const char *dot = strrchr(filename, '.');
    if (!dot || dot == filename) 
        return 0;
    
    dot++;
    
    return strcmp(dot, ext) == 0; // case sensitive
}



void findall(const char *path, const char *ext) {
    DIR *dir;
    dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Error opening directory '%s': %s\n", path, strerror(errno));
        return;
    }

    struct dirent *entry;
    struct stat statbuf;
    char path_buf[PATH_MAX];


    while ((entry = readdir(dir)) != NULL) {
        // Skip current directory and parent directory entries.
        if (entry->d_name[0] == '.' && 
            (entry->d_name[1] == '\0' || 
            (entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
             continue;
        
        // Build the full pathname.
        snprintf(path_buf, sizeof(path_buf), "%s/%s", path, entry->d_name);
        
        if (lstat(path_buf, &statbuf) == -1) {
            fprintf(stderr, "Can not get stats for '%s': %s\n", path_buf, strerror(errno));
            continue;
        }

        // If it is a dir, rec search.
        if (S_ISDIR(statbuf.st_mode)) findall(path_buf, ext);

        // regular file
        else if (S_ISREG(statbuf.st_mode)) {
            // check ext
            if (filename_has_ext(entry->d_name, ext)) {
                serialNum++;
                const char *userlogin = getLoginByUID(statbuf.st_uid);
                printf("%-4d      : %-15s                %-8lu                %s\n", serialNum, userlogin, (unsigned long) statbuf.st_size, path_buf);
            }
        }
        // else ignore
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <directory> <extension>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    uidmapping();
    
    printf("NO        : OWNER                          SIZE                    NAME\n");
    printf("--          -----                          ----                    ----\n");
    
    // Rec search.
    findall(argv[1], argv[2]);
    
    // clean up
    free(uidMapArr);
    return EXIT_SUCCESS;
}
