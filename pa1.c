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
