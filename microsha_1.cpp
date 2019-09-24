#include <stdio.h>
#include <string.h>
#include <glob.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include <sys/wait.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/times.h>
#include <signal.h>

using namespace std;

void EPIP(vector< vector<char *> > & argv, int n) {
    for (int i = 0; i < n - 1; i++) {
        int fd[2];
	pipe(fd);
	pid_t pid = fork();
        if (pid == 0) {
	    dup2(fd[1], 1);
	    close(fd[0]);
	    execvp(argv[i][0], &argv[i][0]);
	    exit(0);
	} else {
	    close(fd[1]);
	    dup2(fd[0], 0);
	}
    }	    
    execvp(argv[n-1][0], &argv[n-1][0]);
    exit(0);
}

int main() {
    signal(2, SIG_IGN);
    uid_t uid = getuid();
    string privilege = uid == 0 ? "!" : ">";
    while (! feof(stdin)) {
	
	vector<char *> toBeFreed;
	char buf[1000];
        char *path;
        path = getwd(buf);
        printf("%s%s ", path, privilege.c_str());
        
	string command;
	getline(cin, command);
	
	int p;
	while ((p = command.find("  ")) != string::npos) {
            command.erase(p, 1);
	}
	if (command[0] == ' ') {
            command.erase(0, 1);
	}
	if (! command.empty() && command[command.length() - 1] == ' ') {
            command.pop_back();
	}
        if (command.empty()) {
	    continue;
	}

        if (command.find("|") == string::npos) {

	    vector <string> placeholder;
	    vector <char *> argv;
	    while ((p = command.find(" ")) != string::npos) {
	        placeholder.push_back(command.substr(0, p));
	        command.erase(0, p + 1);
	    }
	    placeholder.push_back(command);

            string fin = "", fout = "";
            for (int i = 0; i < placeholder.size(); ) {
	        if (placeholder[i] == ">") {
		    if (placeholder.size() > i + 1) fout = placeholder[i + 1];
		    placeholder.erase(placeholder.begin() + i);
		    placeholder.erase(placeholder.begin() + i);
	        } else {
		    i++;
		}
	    }
            for (int i = 0; i < placeholder.size(); ) {
	        if (placeholder[i] == "<") {
		    if (placeholder.size() > i + 1) fin = placeholder[i + 1];
		    placeholder.erase(placeholder.begin() + i);
		    placeholder.erase(placeholder.begin() + i);
	        } else {
		    i++;
		}
	    }
	    
	  
            for (int i = 0; i < placeholder.size(); i++) {
                if (placeholder[i].find('*') != string::npos || placeholder[i].find('?') != string::npos) {    
		    const char *pattern = placeholder[i].c_str();
		    glob_t globResults;
		    int globReturn = glob(pattern, 0, NULL, &globResults);
		    if (globReturn == 0) {
		        for (int j = 0; j < globResults.gl_pathc; j++) {
			    char *buf = (char *)malloc((strlen(globResults.gl_pathv[j]) + 2) * sizeof(char));
			    strcpy(buf, globResults.gl_pathv[j]);
			    argv.push_back(buf);
			    toBeFreed.push_back(buf);
			}	
		    } else {
		        argv.push_back((char *)placeholder[i].c_str());
		    }
		    globfree(&globResults);
	        } else {	
	            argv.push_back((char *)placeholder[i].c_str());
	        }
            }
            argv.push_back(NULL);
	    
	    bool isTime = false;
            if (placeholder[0] == "time") {
		isTime = true;
	    }

	    pid_t pid = fork();
	    if (pid == 0) {
		signal(2, SIG_DFL);
	        if (! fout.empty()) {
	            close(1);
		    dup2(open(fout.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666), 1);
	        }
                if (! fin.empty()) {
                    close(0);
		    dup2(open(fin.c_str(), O_RDWR|O_CREAT, 0666), 0);
	        }  

		if (argv.size() > 1 + isTime) execvp(argv[isTime], &argv[isTime]);
		exit(0);
	    } else {
		tms buf_start, buf_end;
		clock_t wall_start, wall_end;

		if (isTime) {
		    wall_start = times(&buf_start);
		}
		
	  	if (placeholder[isTime] == "exit") {
	            exit(0);
	 	} else if (placeholder[isTime] == "cd") {
		    if (argv[1 + isTime] != NULL) {
			chdir(argv[1 + isTime]);
		    } else {
			chdir(::getenv( "HOME" )); 
		    }
		}

                int status;
	        wait(&status);

		if (isTime) {
		    wall_end = times(&buf_end);
		    fprintf(stderr, "Realtime:\t%lf\nSystemtime:\t%lf\nUsertime:\t%lf\n", 
				    10000 * (double)(wall_end - wall_start) / CLOCKS_PER_SEC, 
				    10000 * (double)(buf_end.tms_cstime - buf_start.tms_cstime) / CLOCKS_PER_SEC, 
				    10000 * (double)(buf_end.tms_cutime - buf_start.tms_cutime) / CLOCKS_PER_SEC);
		}
	    }

	} else {

	    int n = count(command.begin(), command.end(), '|') + 1;
            vector <string> placeholder;
            vector < vector <char *> > argv(n); 
	    while ((p = command.find(" ")) != string::npos) {
	        placeholder.push_back(command.substr(0, p));
	        command.erase(0, p + 1);
	    }
	    placeholder.push_back(command);
            string fin = "", fout = "";

	    int j = 0;
	    for (int i = 0; i < placeholder.size(); i++) {
	        if (placeholder[i] == "|") {
		    argv[j].push_back(NULL);
		    j++;
		} else if (placeholder[i] == ">") {
		    if (placeholder.size() > i + 1 && j == n - 1) fout = placeholder[i + 1];
		    placeholder.erase(placeholder.begin() + i + 1);
		} else if (placeholder[i] == "<") {
		    if (placeholder.size() > i + 1 && j == 0) fin = placeholder[i + 1];
		    placeholder.erase(placeholder.begin() + i + 1);
		} else if (placeholder[i].find('*') != string::npos || placeholder[i].find('?') != string::npos) {
		    const char *pattern = placeholder[i].c_str();
		    glob_t globResults;
		    int globReturn = glob(pattern, 0, NULL, &globResults);
		    if (globReturn == 0) {
		        for (int k = 0; k < globResults.gl_pathc; k++) {
			    char *buf = (char *)malloc((strlen(globResults.gl_pathv[k]) + 2) * sizeof(char *));
			    strcpy(buf, globResults.gl_pathv[k]);
			    argv[j].push_back(buf);
			    toBeFreed.push_back(buf);
			}	
		    } else {
		        argv[j].push_back((char *)placeholder[i].c_str());
		    }
		    globfree(&globResults);
		} else {
		    argv[j].push_back((char *)placeholder[i].c_str());
		}
	    }
            argv[j].push_back(NULL);
	    
	    pid_t pid = fork();
	    if (pid == 0) {
		signal(2, SIG_DFL);
	        if (! fout.empty()) {
	            close(1);
		    dup2(open(fout.c_str(), O_RDWR|O_CREAT|O_TRUNC, 0666), 1);
	        }
                if (! fin.empty()) {
                    close(0);
		    dup2(open(fin.c_str(), O_RDWR|O_CREAT, 0666), 0);
	        }  
		EPIP(argv, n);
	    } else {
		int status;
		wait(&status);
	    }
	}
	for (int i = 0; i < toBeFreed.size(); i++) free(toBeFreed[i]);
    }
    cout << endl;
    return 0;
}
