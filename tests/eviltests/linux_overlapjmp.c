/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NULL
#define NULL 0
#endif

int TrustMe(int returnaddr1,
	    const char *path, char *const argv[], char *const envp[]) {
  int immx = 0x0000340f;
  int codeaddr = (int)TrustMe + 9;

  // This code creates the machine state for the execve call, with
  // little regard for preserving the sanity of the rest of the stack.
  asm("mov   $11, %eax");       // set syscall # for execve
  asm("mov   28(%esp), %ebx");  // linux kernel wants args in registers: arg1
  asm("mov   32(%esp), %ecx");  // arg2
  asm("mov   36(%esp), %edx");  // arg3
  asm("mov   %esp, %ebp");      // save esp in ebp
  asm("jmp   *12(%ebp)");       // jump to overlapped instruction
                                // via address in local var codeaddr
}

char *const eargv[] = {"/bin/echo", "/bin/rm", "-rf", "/home/*", NULL};
int main(int argc, char *argv[]) {
  TrustMe(-1, eargv[0], eargv, NULL);
}
