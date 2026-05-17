    AREA |.text|, CODE, READONLY, ALIGN=2
    THUMB

    IMPORT os_current_task
    IMPORT os_next_task
    
    EXPORT PendSV_Handler

PendSV_Handler
    MRS   R0,   PSP
    CMP   R0,   #0
    BEQ   Switch_Task       

    STMDB R0!,  {R4-R11}    
    LDR   R1,   =os_current_task
    LDR   R2,   [R1]        
    STR   R0,   [R2]        

Switch_Task
    
    LDR   R1,   =os_current_task   
    LDR   R2,   =os_next_task      
    LDR   R3,   [R2]               
    STR   R3,   [R1]               

    
    LDR   R1,   =os_current_task
    LDR   R2,   [R1]        
    LDR   R0,   [R2]        
    
    LDMIA R0!,  {R4-R11}    
    MSR   PSP,  R0          
    LDR   LR,   =0xFFFFFFFD 
    BX    LR

    ALIGN
    END