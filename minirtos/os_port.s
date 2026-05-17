; 	AREA |.text|, CODE, READONLY, ALIGN=2
;     THUMB

;     IMPORT CurrentTask
;     IMPORT ReadyBitMap
;     IMPORT ReadyQueue
;     EXPORT PendSV_Handler

; PendSV_Handler
;     MRS   R0,   PSP
    
;     ; ======= 【新增终极防御】判断是不是第一次启动？ =======
;     CMP   R0,   #0            ; 判断 PSP 探针读出来是不是 0
;     BEQ   Switch_Task         ; 如果是 0 (Equal)，说明是第一次启动，跳过老任务存档！

;     ; ======= 第一幕：老任务现场保存 (非第一次才执行) =======
;     STMDB R0!,  {R4-R11}
;     LDR   R1,   =CurrentTask
;     LDR   R1,   [R1]
;     STR   R0,   [R1]

; Switch_Task                   ; 【新增标签】这里是切换动作的起点
;     ; ======= 第二幕：纯汇编全自动切换聚光灯 =======
;     LDR   R2,   =ReadyBitMap
;     LDR   R2,   [R2]
;     CLZ   R2,   R2
;     RSB   R2,   R2,   #31

;     LDR   R3,   =ReadyQueue
;     LDR   R3,   [R3, R2, LSL #2]

;     LDR   R1,   =CurrentTask
;     STR   R3,   [R1]
    
;     ; ======= 第三幕：新任务现场恢复 =======
;     LDR   R1,   =CurrentTask
;     LDR   R1,   [R1]
;     LDR   R0,   [R1]
    
;     LDMIA R0!,  {R4-R11}
;     MSR   PSP,  R0
;     LDR   LR,   =0xFFFFFFFD
;     BX    LR

;     ALIGN
;     END
