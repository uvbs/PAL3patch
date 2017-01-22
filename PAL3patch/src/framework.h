#ifndef PAL3PATCH_FRAMEWORK_H
#define PAL3PATCH_FRAMEWORK_H

extern void memcpy_to_process(unsigned dest, const void *src, unsigned size);
extern void memcpy_from_process(void *dest, unsigned src, unsigned size);
extern void make_branch(unsigned addr, unsigned char opcode, const void *jtarget, unsigned size);
extern void *get_branch_jtarget(unsigned addr, unsigned char opcode);
extern void make_jmp(unsigned addr, const void *jtarget);
extern void make_call(unsigned addr, const void *jtarget);
extern void make_call_batch(unsigned *addr_list, int count, const void *jtarget);
extern void check_code(unsigned addr, const void *code, unsigned size);
extern unsigned get_module_base(const char *modulename);
extern void *get_func_address(const char *dllname, const char *funcname);
extern void hook_iat(void *iatbase, void *oldptr, void *newptr);
extern void *hook_import_table(void *image_base, const char *dllname, const char *funcname, void *newptr);
#define SIMPLE_PATCH(addr, oldcode, newcode, size) \
    do { \
        check_code(addr, oldcode, size); \
        memcpy_to_process(addr, newcode, size); \
    } while (0)
#define SIMPLE_PATCH_NOP(addr, oldcode, size) \
    do { \
        unsigned char __nop[size]; \
        memset(__nop, NOP, size); \
        SIMPLE_PATCH(addr, oldcode, __nop, size); \
    } while (0)
#define INIT_WRAPPER_CALL(wrapper_func, caller_list...) \
    do { \
        unsigned __caller_list[] = caller_list; \
        make_call_batch(__caller_list, sizeof(__caller_list) / sizeof(__caller_list[0]), (wrapper_func)); \
    } while (0)
#define INIT_WRAPPER_VFPTR(wrapper_func, vfptr_addr) \
    do { \
        void *__func_ptr = (wrapper_func); \
        memcpy_to_process((vfptr_addr), &__func_ptr, sizeof(__func_ptr)); \
    } while (0)
#define PATCH_FLOAT_MEMREF_PTR(float_addr, instr_list...) \
    do { \
        float *__new_value_addr = (float_addr); \
        unsigned __instr_addr[] = instr_list; \
        int __instr_addr_cnt = sizeof(__instr_addr) / sizeof(__instr_addr[0]); \
        unsigned *__instr_addr_ptr; \
        for (__instr_addr_ptr = __instr_addr; __instr_addr_ptr < __instr_addr + __instr_addr_cnt; __instr_addr_ptr++) { \
            memcpy_to_process(*__instr_addr_ptr + 2, &__new_value_addr, sizeof(__new_value_addr)); \
        } \
    } while (0)
#define PATCH_FLOAT_MEMREF_EXPR(expr, instr_list...) \
    do { \
        static float __expr_value; \
        __expr_value = (expr); \
        PATCH_FLOAT_MEMREF_PTR(&__expr_value, instr_list); \
    } while (0)


#endif
