;;! target = "x86_64"

(module
    (func (result i64)
        (i64.const 1)
        (i64.extend32_s)
    )
)
;;    0:	 55                   	push	rbp
;;    1:	 4889e5               	mov	rbp, rsp
;;    4:	 4883ec08             	sub	rsp, 8
;;    8:	 4c893424             	mov	qword ptr [rsp], r14
;;    c:	 48c7c001000000       	mov	rax, 1
;;   13:	 4863c0               	movsxd	rax, eax
;;   16:	 4883c408             	add	rsp, 8
;;   1a:	 5d                   	pop	rbp
;;   1b:	 c3                   	ret	
