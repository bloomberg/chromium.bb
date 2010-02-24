.text
.globl test_tls_asm

test_tls_asm:
  push %rdi

  /*
   * This code must match the code in nacl_syscall_64.S
   */
// mov eax, OFFSET FLAT:tlsValue
  leaq tlsValue(%rip), %rax

// mov edx, eax
  mov $0x00000fff, %edx
  and %edx, %eax
  mov %eax, %edx

// mov ecx, DWORD PTR _tls_index
  mov _tls_index(%rip), %ecx

// mov rax, QWORD PTR gs:88
  movq %gs:0x58, %rax

// mov rax, QWORD PTR [rax+rcx*8]
  movq (%rax,%rcx,8), %rax

// mov rax, QWORD PTR [rdx+rax]
  movq (%rdx,%rax), %rax

  pop %rdi
  ret
