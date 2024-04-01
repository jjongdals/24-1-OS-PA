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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

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
	int status, result = 0;
	int nr_pipes = 0; //파이프의 갯수
	//파이프의 위치를 저장하는 배열 생성, 동적으로 할당해줌, nr_tokens - 1 을 해준 이유는 파이프 이후 추가적인 명령어가 들어오기 때문
	int *pipe_loc = malloc(sizeof(int) * (nr_tokens - 1));
	// pipe가 있는지 체크하고 파이프의 위치를 반환
	for(int i = 0; i < nr_tokens; i++) {
		if (strcmp(tokens[i], "|") == 0) {
			pipe_loc[nr_pipes++] = i;
		}
	}
	//파이프가 있는 경우, pipe_loc의 길이가 0이 아니면 파이프 존재
	if(nr_pipes > 0) {
		int idx = 0; // execvp에 넣어줄 변수
		// 파일 디스크립터 생성 (파이프의 개수만큼 동적으로 생성해줌 -> 파이프가 여러개 일수도 있기 때문에)
		int (*pipefd)[2] = malloc(2 * sizeof(int) * nr_pipes);
		for (int i = 0; i < nr_pipes; i++) {
			if(pipe(pipefd[i]) == -1) exit(1); // 파이프 반환값이 -1이면 실패!
		}
		//파이프가 있을 떄도 기본 동작은 같음
		for(int i = 0; i <= nr_pipes; i++) {
			//cd일 경우 체크
			if (strcmp(tokens[idx], "cd") == 0) {
				//디렉토리를 변경할 경우, 두번째 토큰에 path가 전달됨 따라서 dir 문자열에 token의 두번째 토큰값 전달
				char *dir = tokens[idx + 1]; 
				//cd나 cd ~ 일 경우 사용자의 홈디렉토리로 변경
				if (tokens[idx + 1] == NULL || strcmp(tokens[idx + 1], "~") == 0) {
					dir = getenv("HOME"); 
				}
				//디렉토리 변경에 실패할 경우엔 -1 반환 아니면 1 반환
				if (chdir(dir) != 0) { 
					return -1; 
				}
				return 1; 
			}
			pid = fork(); // 포크해서 자식 프로세스를 만들고
			if (pid == 0) {
				//파이프 input pipefd[0]이 input임
				if (i > 0) {
					dup2(pipefd[i-1][0], STDIN_FILENO);
				}
				//파이프 output
				if (i < nr_pipes) {
					dup2(pipefd[i][1], STDOUT_FILENO);
				}
				//파이프 닫기
				for (int j = 0; j < i; j++) {
					close(pipefd[j][0]);
					close(pipefd[j][1]);
				}
				// execvp 실행을 위해 tokens 초기화 필수
				if (i < nr_tokens) {
					tokens[pipe_loc[i]] = NULL;
				}
				else {
					tokens[nr_tokens] = NULL;
				}
				result = execvp(tokens[idx], &tokens[idx]);
				//execvp 반환값으로 성공여부 체크
				if (result == -1) {
					exit(1);
				}
				else {
					exit(0);
				}				
			}
			// 파이프 다음이 실행되기 때문에 idx는 토큰의 위치에서 +1
			idx = pipe_loc[i] + 1;
		}
		// 부모 프로세스 파일 디스크립터 닫기
		for (int i = 0; i < nr_pipes; i++) {
			close(pipefd[i][0]);
			close(pipefd[i][1]);
		}
		// 자식 프로세스가 끝날 때까지 부모 프로세스는 대기
		while (wait(&status) > 0);
		// 메모리 할당 해제
		free(pipe_loc);
		free(pipefd);
		if(status == 0) {
			return 1;
		}
		else {
			return -1;
		}
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
