#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <vmalloc.h>

#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#elif !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS 0x20
#endif

/* ========== 全局变量 ========== */

/** 全局虚拟内存管理器 */
static vm_manager_t *g_vm_mgr = NULL;


/* ========== 内部辅助函数 ========== */

/** 在管理器中查找指定地址的区间 */
static vm_region_t *_vmalloc_find_region(uintptr_t addr) {
    if(g_vm_mgr == NULL) {
        return NULL;
    }

    vm_region_t *region = g_vm_mgr->regions_head;
    while (region != NULL) {
        if(region->start_addr == addr) {
            return region;
        }
        region = region->next;
    }
    return NULL;
}


/** 检查大小是否页对齐 */
static int _vmalloc_is_page_aligned(size_t size) {
    return (size % PAGE_SIZE) == 0;
}


/* ========== 公共接口实现 ========== */

int vmalloc_init(void) {
    if(g_vm_mgr != NULL) {
        return 0;
    }

    g_vm_mgr = (vm_manager_t *)malloc(sizeof(vm_manager_t));
    if(g_vm_mgr == NULL) {
        ERROR_PRINT("[ERROR] failed to allocate vm_manager");
        return -1;
    }

    g_vm_mgr->regions_head = NULL;
    g_vm_mgr->region_count = 0;
    g_vm_mgr->total_allocated = 0;

    DEBUG_PRINT("vmalloc_init: success");

    return 0;
}

void *vmalloc(void *addr, size_t length) {
    // 检查初始化
    if(g_vm_mgr == NULL) {
        if(vmalloc_init() != 0) {
            ERROR_PRINT("vmalloc: initialization failed");
            return NULL;
        }
    }

    // 参数检查
    if(length == 0 || !_vmalloc_is_page_aligned(length)) {
        ERROR_PRINT("vmalloc: invalid length %zu (not page-aligned)", length);
        errno = EINVAL;
        return NULL;
    }

    DEBUG_PRINT("vmalloc: requestion %zu bytes from %p", length, addr);

    // 调用 mmap 分配内存
    void *ptr = mmap(
        addr,                           // 建议地址（NULL让OS自动分配）
        length,                         // 大小
        PROT_READ | PROT_WRITE,         // 可读写
        MAP_PRIVATE | MAP_ANONYMOUS,    // 私有匿名
        -1,                             // 无关联文件
        0                               // 无偏移
    );

    if(ptr == MAP_FAILED) {
        ERROR_PRINT("vmalloc: mmap failed fot %zu bytes: %s",
            length, strerror(errno));
        return NULL;
    }

    DEBUG_PRINT("vmalloc: mmap succeeded, ptr: %p", ptr);

    // 创建 vm_region 记录
    vm_region_t *region = (vm_region_t *)malloc(sizeof(vm_region_t));
    if(region == NULL) {
        ERROR_PRINT("vmalloc: failed to allocate region metadata");
        munmap(ptr, length);
        return NULL;
    }

    region->start_addr = (uintptr_t)ptr;
    region->length = length;
    region->prot_flags = PROT_READ | PROT_WRITE;
    region->map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

    // 添加到链表头部
    region->next = g_vm_mgr->regions_head;
    g_vm_mgr->regions_head = region;

    // 更新统计信息
    g_vm_mgr->total_allocated += length;
    g_vm_mgr->region_count ++;

    DEBUG_PRINT("vmalloc: region added, total=%zu, count=%zu",
        g_vm_mgr->total_allocated, g_vm_mgr->region_count);

    return ptr;
}

int vmfree(void *addr, size_t length) {
    // 检查管理器初始化
    if (g_vm_mgr == NULL) {
        ERROR_PRINT("vmfree: vmalloc not initialized");
        return -1;
    }

    // 查找区间
    uintptr_t addr_int = (uintptr_t)addr;
    vm_region_t *region = _vmalloc_find_region(addr_int);
    if (region == NULL) {
        ERROR_PRINT("vmfree: region not found at %p", addr);
        errno = EINVAL;
        return -1;
    }

    // 验证长度
    if (region->length != length) {
        ERROR_PRINT("vmfree: length mismath at %p: expected %zu, got %zu",
            addr, length, region->length);
        errno = EINVAL;
        return -1;
    }

    // 调用 munmap释放内存
    if (munmap(addr, length) != 0) {
        ERROR_PRINT("vmfree: munmap failed at %p: %s",
            addr, strerror(errno));
            return -1;
    }

    DEBUG_PRINT("vm_free: munmap succeeded");

    // 从链表移除
    if (region == g_vm_mgr->regions_head) {
        g_vm_mgr->regions_head = region->next;
    } else {
        vm_region_t *prev = g_vm_mgr->regions_head;
        while (prev != NULL && prev->next != region) {
            prev = prev->next;
        } 
        
        if(prev != NULL) {
            prev->next = region->next;
        }
    }
    free(region);

    // 更新管理器统计信息
    g_vm_mgr->region_count --;
    g_vm_mgr->total_allocated -= length;

    DEBUG_PRINT("vmfree: region removed, total=%zu, count=%zu",
        g_vm_mgr->total_allocated, g_vm_mgr->region_count);

    return 0;
}

size_t vmalloc_total_allocated(void) {
    if (g_vm_mgr == NULL) {
        return 0;
    }
    return g_vm_mgr->total_allocated;
}

size_t vmalloc_region_count(void) {
    if (g_vm_mgr == NULL) {
        return 0;
    }
    return g_vm_mgr->region_count;
}

int vmalloc_cleanup(void) {
    if (g_vm_mgr == NULL) {
        return 0;
    }

    DEBUG_PRINT("vmalloc_cleanup: cleaning up %zu regions",
        g_vm_mgr->region_count);

    // 遍历所有区间
    vm_region_t *region = g_vm_mgr->regions_head;
    while (region != NULL) {
        vm_region_t *next = region->next;

        // 释放内存
        if (munmap((void *)region->start_addr, region->length) != 0) {
            ERROR_PRINT("vmalloc_cleanup: munmap failed at %p",
                (void *)region->start_addr);
        }
        free(region);

        // 释放元数据
        region = next;
    }

    // 释放管理器本身
    free(g_vm_mgr);
    g_vm_mgr = NULL;

    DEBUG_PRINT("vmalloc_cleanup: done");

    return 0;
}

void vmalloc_dump(void) {
    if (g_vm_mgr == NULL) {
        printf("vmalloc_dump: not initialized\n");
        return ;
    }

    printf("=== VM Regions Dump ===\n");
    printf("Total regions: %zu\n", g_vm_mgr->region_count);
    printf("Total allocated: %zu bytes\n\n", g_vm_mgr->total_allocated);

    vm_region_t *region = g_vm_mgr->regions_head;
    int i = 0;
    while (region != NULL) {
        printf("[Region %d] addr=0x%lx size=%zu (%zu pages)\n",
               i++,
               region->start_addr,
               region->length,
               region->length / PAGE_SIZE);
        region = region->next;
    }

    printf("======================\n");
}