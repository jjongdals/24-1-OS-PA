/**********************************************************************
 * Copyright (c) 2020-2024
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/
typedef unsigned char bool;
#define true	1
#define false	0

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "list_head.h"
#include "parser.h"

#define CHILD 0
// alias 구현을 위한 구조체 선언, name은 사용자가 지정한 변수명, command는 대응되는 명령어
typedef struct alias {
	struct list_head list;
	char *name;
	char *command;
} alias_entry;
//stack 자료구조로 alias 사용
LIST_HEAD(stack);

/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */
int run_command(int nr_tokens, char *tokens[])
{
	if (strcmp(tokens[0], "exit") == 0) return 0; // exit일 경우
	pid_t pid;
	int status, status2, result = 0;
	char *alias_tokens[MAX_NR_TOKENS] = { NULL }; // parse_command에 넣기 위한 token
	// alias가 있다면 alias 처리
	if(!(list_empty(&stack))) {
		alias_entry * pos;
		//먼저 alias 순회
		list_for_each_entry(pos, &stack, list) {
			// 일단 nr_tokens만큼 처리하고 여기서 alias로 대체된 문자열이 있다면 그걸 대신해서 실행
			for (int i = 0; i < nr_tokens ; i++) {
				//i번째 token이 alias로 지정한 이름과 같다면? 거기에 있는 command를 실행
				if(strcmp(tokens[i], pos->name) == 0) {
					//execute command는 alias의 명령어임 name 같은 걸 이걸로 대체
					char *execute_command = malloc(sizeof(pos->command) + 1);
					//execute_command를 자름
					int nr_commands = parse_command(execute_command, alias_tokens);
					for (int j = 0; j < nr_commands; j++, i++) {
                        tokens[i] = strdup(alias_tokens[j]); 
                    }
					for (int k = 0; k < nr_commands; k++) {
                        free(alias_tokens[k]);
                    }
					break;
				}
			}
		}
	}
	int nr_pipes = 0; //파이프의 갯수
	int pipe_loc = 0; //파이프 위친
	// pipe가 있는지 체크하고 파이프의 위치를 반환
	for(int i = 0; i < nr_tokens; i++) {
		if (strcmp(tokens[i], "|") == 0) {
			pipe_loc = i;
			nr_pipes++;
		}
	}
	//파이프가 있는 경우, nr_pipes가 0이 아니면 파이프 존재
	if(nr_pipes > 0) {
		if (strcmp(tokens[0], "cd") == 0) {
			//디렉토리를 변경할 경우, 두번째 토큰에 path가 전달됨 따라서 dir 문자열에 token의 두번째 토큰값 전달
			char *dir = tokens[1]; 
			//cd나 cd ~ 일 경우 사용자의 홈디렉토리로 변경
			if (tokens[1] == NULL || strcmp(tokens[1], "~") == 0) {
				dir = getenv("HOME"); 
			}
			//디렉토리 변경에 실패할 경우엔 -1 반환 아니면 1 반환
			if (chdir(dir) != 0) { 
				return -1; 
			}
			return 1; 
		}
		int pipefd[2];
		if (pipe(pipefd) == -1) exit(1);
		tokens[pipe_loc] = NULL;
		// 왼쪽 자식 프로세스를 포크해줌
		pid_t left_pid = fork();
		// 부모 먼저 실행
		if (left_pid != CHILD) {
			close(pipefd[STDOUT_FILENO]);
			// 오른쪽 포크해줌
			pid_t right_pid = fork();
			if (right_pid != CHILD) {
				close(pipefd[STDIN_FILENO]);
				waitpid(left_pid, &status, 0); // wait pid (that executes left command)
				waitpid(right_pid, &status2, 0);
			}
			//오른쪽 자식 프로세스
			else if (right_pid == CHILD) {
				dup2(pipefd[STDIN_FILENO], STDIN_FILENO);
				close(pipefd[STDOUT_FILENO]);
				result = execvp(tokens[pipe_loc+1], &tokens[pipe_loc+1]);
				if (result == -1) {
					fprintf(stderr, "Unable to execute %s\n", tokens[pipe_loc+1]);
					close(pipefd[STDIN_FILENO]);
					exit(1);
				}
				exit(0);
			}
		}
		else if (left_pid == CHILD){
			dup2(pipefd[STDOUT_FILENO], STDOUT_FILENO);
			close(pipefd[STDIN_FILENO]);
			result = execvp(tokens[0], &tokens[0]);
			if (result == -1) {
				fprintf(stderr, "Unable to execute %s\n", tokens[0]);
				close(pipefd[STDOUT_FILENO]);
				exit(1);
			}
			exit(0);
		}
		return 1;
	} 
	//파이프가 존재하지 않는 경우
	else {
		// cd일 경우
		if (strcmp(tokens[0], "cd") == 0) {
			//디렉토리를 변경할 경우, 두번째 토큰에 path가 전달됨 따라서 dir 문자열에 token의 두번째 토큰값 전달
			char *dir = tokens[1]; 
			//cd나 cd ~ 일 경우 사용자의 홈디렉토리로 변경
			if (tokens[1] == NULL || strcmp(tokens[1], "~") == 0) {
				dir = getenv("HOME"); 
			}
			//디렉토리 변경에 실패할 경우엔 -1 반환 아니면 1 반환
			if (chdir(dir) != 0) { 
				return -1; 
			}
			return 1; 
		} 
		//alias 일 경우 내부 명령어기 때문에 fork 할 필요는 없음
		if (strcmp(tokens[0], "alias") == 0) {
			//alias 명령어를 추가하는 케이스도 생각해야 함 -> 이 때는 nr_tokens가 2개 이상임
			if (nr_tokens > 1) {
				//input size는 토큰길이의 맥스값이랑 토큰 개수의 맥스값을 곱해줌
				int input_size = MAX_TOKEN_LEN * MAX_NR_TOKENS;
				char *input_command = malloc(input_size);
				// memset으로 문자열 뒤를 0으로 초기화해줌 -> strcpy나 strlen을 쓰기 위해서..
				if (input_command != NULL) memset(input_command, 0, input_size);
				alias_entry * add_alias = (alias_entry*)malloc(sizeof(alias_entry));
				// alias xyz hello world가 들어올 땐 xyz 뒤의 hello world 전체가 들어옴 따라서 이걸 전부 다 고려 (공백도 두번 스페이스 되더라도 한개로 처리)
				for (int i = 2; i < nr_tokens; i++) {
					strcat(input_command, tokens[i]);
					// input command에 공백도 고려해서 넘겨줌
					if (i < nr_tokens - 1) {
						strcat(input_command, " ");
					}	
				}
				//command 문자열은 string으로 전달되기 때문에 동적할당으로 메모리할당을 해줘야함
				add_alias->command = malloc(strlen(input_command)+1);
				add_alias->name = malloc(strlen(tokens[1])+1);
				//alias name에는 token[1]이 들어옴 
				strcpy(add_alias->name, tokens[1]);
				//alias command에는 input command가 들어옴
				strcpy(add_alias->command, input_command);
				//pa0처럼 stack에다 추가
				list_add(&add_alias->list, &stack);
			}
			// alias 목록 리스트 출력할 케이스
			else {
				alias_entry *alias_list;
				// alias에 저장된 명령어를 출력할 때 , readme에는 역순으로 출력함 따라서 reverse로 접근
				list_for_each_entry_reverse(alias_list, &stack, list) {
					fprintf(stderr, "%s: %s\n", alias_list->name, alias_list->command);
				}
			}
			return 1;
		}
		//fork의 반환을 pid에 저장
		pid = fork();
		//fork의 반환값이 0이라면, 자식 프로세스임
		if(pid == 0){
			//execvp의 반환값을 result에 저장, result가 -1이면 실패
			result = execvp(tokens[0], tokens);
			//execvp가 실패하면 에러 반환 및 부모 프로세스로 돌아감
			if (result == -1) {
				exit(1);	
			}
			else {
			// 성공하면 exit(0)으로 부모 프로세스에게 정보 반환
				exit(0);
			}
		}	
		//자식 프로세스가 끝날 때까지 부모 프로세스는 대기
		wait(&status);
		//stauts가 0이면 성공, 아니면 실패
		if(status == 0) {
			return 1;
		}
		else {
			return -1;
		}
	}	
	return -1;
}
/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
void finalize(int argc, char * const argv[])
{
}
