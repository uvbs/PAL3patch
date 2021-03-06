#include "common.h"

// file functions
#define MY_REG_FILE "save\\registry.txt"
#define MAX_REG_ITEM 100

struct reg_item {
    char *key1;
    char *key2;
    unsigned val;
};

static struct reg_item reg[MAX_REG_ITEM];
static int nr_reg;

static struct reg_item reg_default[] = {
    { "SOFTWARE\\SOFTSTAR\\PAL3_MOVIE" , "MOVIE_A"  },
    { "SOFTWARE\\SOFTSTAR\\PAL3_MOVIE" , "MOVIE_B"  },
    { "SOFTWARE\\SOFTSTAR\\PAL3_SAVE"  , "SEEK"     },
    { "SOFTWARE\\SOFTSTAR\\PAL3_S_GAME", "S_GAME_A" },
    { "SOFTWARE\\SOFTSTAR\\PAL3_S_GAME", "S_GAME_B" },
    { "SOFTWARE\\SOFTSTAR\\PAL3_S_GAME", "S_GAME_C" },
    
    { NULL, NULL } // EOF
};

static int reg_cmp(const void *a, const void *b)
{
    const struct reg_item *pa = a, *pb = b;
    int ret = strcmp(pa->key1, pb->key1);
    if (ret) return ret;
    return strcmp(pa->key2, pb->key2);
}
static int alloc_reg()
{
    if (nr_reg >= MAX_REG_ITEM) fail("too many registry items.");
    return nr_reg++;
}
static void load_reg()
{
    nr_reg = 0;
    FILE *fp = fopen(MY_REG_FILE, "r");
    if (!fp) return;
    char buf1[MAXLINE], buf2[MAXLINE];
    unsigned val;
    int ret;
    fscanf(fp, "\xEF\xBB\xBF");
    while (1) {
        // skip comment lines
        char tmp[3];
        while (fscanf(fp, " %2[;#]%*[^\n] ", tmp) == 1);
        
        // read registry tuple
        ret = fscanf(fp, MAXLINEFMT MAXLINEFMT "%x ", buf1, buf2, &val);
        if (ret != 3) break;
        int id = alloc_reg();
        reg[id].key1 = strdup(buf1);
        reg[id].key2 = strdup(buf2);
        reg[id].val = val;
    }
    fclose(fp);
    if (ret != 0 && ret != EOF) fail("can't parse registry file.");
    qsort(reg, nr_reg, sizeof(struct reg_item), reg_cmp);
    int i;
    for (i = 1; i < nr_reg; i++) {
        if (reg_cmp(&reg[i - 1], &reg[i]) == 0) {
            fail("duplicate registry key:\n  '%s'\n  '%s'", reg[i].key1, reg[i].key2);
        }
    }
}
static void save_reg()
{
    PrepareDir(); // call PrepareDir() to create the "./save" Directory
    qsort(reg, nr_reg, sizeof(struct reg_item), reg_cmp);
    FILE *fp = fopen(MY_REG_FILE, "w");
    if (fp) {
        fputs("\xEF\xBB\xBF", fp);
        fprintf(fp, "; PAL3 registry save file\n");
        fprintf(fp, "; generated by PAL3patch %s (built on %s)\n", patch_version, build_date);
        SYSTEMTIME SystemTime;
        GetLocalTime(&SystemTime);
    	fprintf(fp, "; last modification: %04hu-%02hu-%02hu %02hu:%02hu:%02hu.%03hu\n", SystemTime.wYear, SystemTime.wMonth, SystemTime.wDay, SystemTime.wHour, SystemTime.wMinute, SystemTime.wSecond, SystemTime.wMilliseconds);
    	fprintf(fp, "\n");
    	fprintf(fp, "%-40s%-15s%s\n", "; HKLM subkey", "name", "value");
        fprintf(fp, "\n");
    
        int i;
        for (i = 0; i < nr_reg; i++) {
            fprintf(fp, "%-40s%-15s%08X\n", reg[i].key1, reg[i].key2, reg[i].val);
        }
        fclose(fp);
    } else {
        try_goto_desktop();
        MessageBoxW(game_hwnd, wstr_cantsavereg_text, wstr_cantsavereg_title, MB_ICONWARNING | MB_TOPMOST | MB_SETFOREGROUND);
    }
}
static void assign_reg(const char *key1, const char *key2, unsigned val)
{
    int i;
    for (i = 0; i < nr_reg; i++) {
        if (strcmp(reg[i].key1, key1) == 0 && strcmp(reg[i].key2, key2) == 0) {
            reg[i].val = val;
            goto done;
        }
    }
    int id = alloc_reg();
    reg[id].key1 = strdup(key1);
    reg[id].key2 = strdup(key2);
    reg[id].val = val;
done:
    save_reg();
}
static int query_reg(const char *key1, const char *key2, unsigned *pval)
{
    int i;
    for (i = 0; i < nr_reg; i++) {
        if (strcmp(reg[i].key1, key1) == 0 && strcmp(reg[i].key2, key2) == 0) {
            *pval = reg[i].val;
            return 1;
        }
    }
    return 0;
}

// no clean up functions, just let these strings leak





// registry functions
static void write_winreg(LPCSTR lpSubKey, LPCSTR lpValueName, DWORD Data)
{
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, lpSubKey, 0, "", REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        RegSetValueEx(hKey, lpValueName, 0, REG_DWORD, (CONST BYTE *) &Data, sizeof(DWORD));
        RegCloseKey(hKey);
    }
}
static void read_winreg(LPCSTR lpSubKey, LPCSTR lpValueName, LPDWORD lpData)
{
    HKEY hKey;
    DWORD dwType = REG_DWORD;
    DWORD dwSize = sizeof(DWORD);
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, lpValueName, NULL, &dwType, (LPBYTE) lpData, &dwSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
        } else {
            *lpData = 0;
        }
    } else {
        *lpData = 0;
    }
}

/*
    0: patch disabled (read and write windows registry only)
    1: read file first, read windows registry when key not exists in file
       write both file and windows registry
    2: read and write file only
*/
#define REGREDIRECT_SYNC 1
#define REGREDIRECT_FILEONLY 2
static int regredirect_flag;


// patch functions
static bool save_dword(LPCSTR lpSubKey, LPCSTR lpValueName, DWORD Data) // WriteReg
{
    // write file
    assign_reg(lpSubKey, lpValueName, Data);
    if (regredirect_flag == REGREDIRECT_SYNC) {
        // always write windows registry is sync is enabled
        write_winreg(lpSubKey, lpValueName, Data);
    }
    return true;
}

static bool query_dword(LPCSTR lpSubKey, LPCSTR lpValueName, LPDWORD lpData) // CheckReg
{
    // read file
    int ret = query_reg(lpSubKey, lpValueName, (unsigned *) lpData);
    if (regredirect_flag == REGREDIRECT_SYNC) {
        if (ret) {
            // if we read data from file
            // sync data to windows registry
            write_winreg(lpSubKey, lpValueName, *lpData);
        } else {
            // if there is no data in file
            // try read registry and sync data to file
            read_winreg(lpSubKey, lpValueName, lpData);
            assign_reg(lpSubKey, lpValueName, *lpData);
        }
    } else {
        if (!ret) *lpData = 0;
    }
    return true;
}




MAKE_PATCHSET(regredirect)
{
    regredirect_flag = flag;
    if (regredirect_flag != REGREDIRECT_SYNC && regredirect_flag != REGREDIRECT_FILEONLY) {
        fail("unknown regredirect_flag %d.", regredirect_flag);
    }
    
    load_reg();
    
    struct reg_item *pdef;
    for (pdef = reg_default; pdef->key1 && pdef->key2; pdef++) {
        DWORD tmp;
        query_dword(pdef->key1, pdef->key2, &tmp);
    }
    
    const unsigned char save_dword_func_magic[] = "\x53\x56\x8B\x74\x24\x0C\x57\x85\xF6\x75\x06\x5F\x5E\x32\xC0\x5B";
    const unsigned save_dword_funcs[] = {
        0x4070A0, 0x437E60, 0x44E2B0, 0x456590,
        0x4B2840, 0x528190, 0x52F570,
    };
    
    unsigned i;
    for (i = 0; i < sizeof(save_dword_funcs) / sizeof(unsigned); i++) {
        check_code(save_dword_funcs[i], save_dword_func_magic, sizeof(save_dword_func_magic) - 1);
        make_jmp(save_dword_funcs[i], save_dword);
    }
    
    const unsigned char query_dword_func_magic[] = "\x51\x8B\x4C\x24\x08\x8D\x44\x24\x08\x50\x6A\x01\x6A\x00\x51\x68";
    const unsigned query_dword_func = 0x456FC0;
    check_code(query_dword_func, query_dword_func_magic, sizeof(query_dword_func_magic) - 1);
    make_jmp(query_dword_func, query_dword);
    
    add_atexit_hook(save_reg);
}
