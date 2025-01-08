#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#define MAX 100

int main(int argc, char* argv[]) {
    // printf("argc = %d\n", argc);

    int u, n = 0;
    FILE* fp, *done_fp;
    char visited[MAX];

    if(argc <=1) {
        printf("Usage: %s <foodule>\n", argv[0]);
        exit(1);
    }
    else {  
        u = atoi(argv[1]); // parent foodule
        fp = fopen("foodep.txt", "r");
        fscanf(fp, "%d", &n);

        if(u > n) {
            printf("Invalid foodule\n");
            exit(1);
        }

    }

    if(argc==2) {
        done_fp = fopen("done.txt", "w");
        for (int i = 0; i < n; i++) {
            fprintf(done_fp, "0");
        }
        fclose(done_fp);
    }

    done_fp = fopen("done.txt", "r");
    fscanf(done_fp, "%s", visited);
    // printf("u = %d\n", u);
    if(visited[u-1] == '1') {
        fclose(done_fp);
        // printf("foo%d is already rebuilt\n", u);
        exit(0);
    }
    
    visited[u-1] = '1';
    // printf("visited = %s\n", visited);
    fclose(done_fp);
    done_fp = fopen("done.txt", "w");
    fprintf(done_fp, "%s", visited);
    fclose(done_fp);



    char temp[MAX];
    char* parent;
    int parent_foodule;

    while(fgets(temp, MAX, fp)){                    // stores the line in parent
        parent = strtok(temp, ":");                   // get first foodule number in temp
        parent_foodule = atoi(parent);
        if(parent_foodule == u){       
            break;
        }
    }

    char* children;
    int child_foodule[MAX];
    int child_count = 0;
    children = strtok(NULL, " ");
    while(children != NULL) {
        child_foodule[child_count] = atoi(children);
        child_count++;
        children = strtok(NULL, " ");
    }

    // printf("parent = %d, child_count = %d\n", parent_foodule, child_count);
    // printf("children = ");  
    // for(int i = 0; i < child_count; i++) {
    //     printf("%d ", child_foodule[i]);
    // }
    // printf("\n");

    pid_t pid;
    for(int i = 0; i < child_count; i++) {
        pid = fork();
        if(pid == 0) {
            char vis;
            // get the char from the string in done.txt
            //open the file
            FILE* done_fp = fopen("done.txt", "r");
            // print the string in done.txt
            fseek(done_fp, child_foodule[i]-1, SEEK_SET);
            fscanf(done_fp, "%c", &vis);
            // printf("visited = %c\n", vis);


            if(vis == '0') {
                char temp[MAX];
                sprintf(temp, "%d", child_foodule[i]);
                execlp("./rebuild", "./rebuild", temp, "22CS30017", NULL);
            }
            else {
                // printf("child %d is already rebuilt\n", child_foodule[i]);
                exit(0);
            }
        }
        else {
            waitpid(pid, NULL, 0);
        }
    }

    if(parent_foodule != 0) printf("foo%d rebuilt", parent_foodule);
    if(child_count >= 0 && child_foodule[0] != 0) {
        printf(" from");
        for(int i = 0; i < child_count; i++) {
            printf(" foo%d", child_foodule[i]);
            if (i < child_count - 1) {
                printf(",");
            }
        }
    }

    if(parent_foodule != 0) printf("\n");
    fclose(fp);
}