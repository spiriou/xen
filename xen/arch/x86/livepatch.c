/*
 * Copyright (C) 2016 Citrix Systems R&D Ltd.
 */

#include <xen/errno.h>
#include <xen/init.h>
#include <xen/lib.h>
#include <xen/mm.h>
#include <xen/pfn.h>
#include <xen/vmap.h>
#include <xen/livepatch_elf.h>
#include <xen/livepatch.h>
#include <xen/sched.h>
#include <xen/vm_event.h>
#include <xen/virtual_region.h>

#include <asm/fixmap.h>
#include <asm/nmi.h>
#include <asm/livepatch.h>
#include <asm/setup.h>

static bool has_active_waitqueue(const struct vm_event_domain *ved)
{
    /* ved may be xzalloc()'d without INIT_LIST_HEAD() yet. */
    return (ved && !list_head_is_null(&ved->wq.list) &&
            !list_empty(&ved->wq.list));
}

/*
 * x86's implementation of waitqueue violates the livepatching safey principle
 * of having unwound every CPUs stack before modifying live content.
 *
 * Search through every domain and check that no vCPUs have an active
 * waitqueue.
 */
int arch_livepatch_safety_check(void)
{
    struct domain *d;

    for_each_domain ( d )
    {
#ifdef CONFIG_MEM_SHARING
        if ( has_active_waitqueue(d->vm_event_share) )
            goto fail;
#endif
#ifdef CONFIG_MEM_PAGING
        if ( has_active_waitqueue(d->vm_event_paging) )
            goto fail;
#endif
        if ( has_active_waitqueue(d->vm_event_monitor) )
            goto fail;
    }

    return 0;

 fail:
    printk(XENLOG_ERR LIVEPATCH "%pd found with active waitqueue\n", d);
    return -EBUSY;
}

int noinline arch_livepatch_quiesce(void)
{
    /* If Shadow Stacks are in use, disable CR4.CET so we can modify CR0.WP. */
    if ( cpu_has_xen_shstk )
        write_cr4(read_cr4() & ~X86_CR4_CET);

    /* Disable WP to allow changes to read-only pages. */
    write_cr0(read_cr0() & ~X86_CR0_WP);

    return 0;
}

void noinline arch_livepatch_revive(void)
{
    /* Reinstate WP. */
    write_cr0(read_cr0() | X86_CR0_WP);

    /* Clobber dirty bits and reinstate CET, if applicable. */
    if ( IS_ENABLED(CONFIG_XEN_SHSTK) && cpu_has_xen_shstk )
    {
        unsigned long tmp;

        reset_virtual_region_perms();

        write_cr4(read_cr4() | X86_CR4_CET);

        /*
         * Fix up the return address on the shadow stack, which currently
         * points at arch_livepatch_quiesce()'s caller.
         *
         * Note: this is somewhat fragile, and depends on both
         * arch_livepatch_{quiesce,revive}() being called from the same
         * function, which is currently the case.
         *
         * Any error will result in Xen dying with #CP, and its too late to
         * recover in any way.
         */
        asm volatile ("rdsspq %[ssp];"
                      "wrssq %[addr], (%[ssp]);"
                      : [ssp] "=&r" (tmp)
                      : [addr] "r" (__builtin_return_address(0)));
    }
}

int arch_livepatch_verify_func(const struct livepatch_func *func)
{
    /* If NOPing.. */
    if ( !func->new_addr )
    {
        /* Only do up to maximum amount we can put in the ->opaque. */
        if ( func->new_size > sizeof(func->opaque) )
            return -EOPNOTSUPP;

        if ( func->old_size < func->new_size )
            return -EINVAL;
    }
    else if ( func->old_size < ARCH_PATCH_INSN_SIZE )
        return -EINVAL;

    return 0;
}

/*
 * "noinline" to cause control flow change and thus invalidate I$ and
 * cause refetch after modification.
 */
void noinline arch_livepatch_apply(struct livepatch_func *func)
{
    uint8_t *old_ptr;
    uint8_t insn[sizeof(func->opaque)];
    unsigned int len;

    old_ptr = func->old_addr;
    len = livepatch_insn_len(func);
    if ( !len )
        return;

    memcpy(func->opaque, old_ptr, len);
    if ( func->new_addr )
    {
        int32_t val;

        BUILD_BUG_ON(ARCH_PATCH_INSN_SIZE != (1 + sizeof(val)));

        insn[0] = 0xe9; /* Relative jump. */
        val = func->new_addr - func->old_addr - ARCH_PATCH_INSN_SIZE;

        memcpy(&insn[1], &val, sizeof(val));
    }
    else
        add_nops(insn, len);

    memcpy(old_ptr, insn, len);
}

/*
 * "noinline" to cause control flow change and thus invalidate I$ and
 * cause refetch after modification.
 */
void noinline arch_livepatch_revert(const struct livepatch_func *func)
{
    memcpy(func->old_addr, func->opaque, livepatch_insn_len(func));
}

/*
 * "noinline" to cause control flow change and thus invalidate I$ and
 * cause refetch after modification.
 */
void noinline arch_livepatch_post_action(void)
{
}

static nmi_callback_t *saved_nmi_callback;
/*
 * Note that because of this NOP code the do_nmi is not safely patchable.
 * Also if we do receive 'real' NMIs we have lost them.
 */
static int mask_nmi_callback(const struct cpu_user_regs *regs, int cpu)
{
    /* TODO: Handle missing NMI/MCE.*/
    return 1;
}

void arch_livepatch_mask(void)
{
    saved_nmi_callback = set_nmi_callback(mask_nmi_callback);
}

void arch_livepatch_unmask(void)
{
    set_nmi_callback(saved_nmi_callback);
}

int arch_livepatch_verify_elf(const struct livepatch_elf *elf)
{

    const Elf_Ehdr *hdr = elf->hdr;

    if ( hdr->e_machine != EM_X86_64 ||
         hdr->e_ident[EI_CLASS] != ELFCLASS64 ||
         hdr->e_ident[EI_DATA] != ELFDATA2LSB )
    {
        printk(XENLOG_ERR LIVEPATCH "%s: Unsupported ELF Machine type\n",
               elf->name);
        return -EOPNOTSUPP;
    }

    return 0;
}

bool arch_livepatch_symbol_ok(const struct livepatch_elf *elf,
                              const struct livepatch_elf_sym *sym)
{
    /* No special checks on x86. */
    return true;
}

bool arch_livepatch_symbol_deny(const struct livepatch_elf *elf,
                                const struct livepatch_elf_sym *sym)
{
    /* No special checks on x86. */
    return false;
}

int arch_livepatch_perform_rel(struct livepatch_elf *elf,
                               const struct livepatch_elf_sec *base,
                               const struct livepatch_elf_sec *rela)
{
    printk(XENLOG_ERR LIVEPATCH "%s: SHT_REL relocation unsupported\n",
           elf->name);
    return -EOPNOTSUPP;
}

int arch_livepatch_perform_rela(struct livepatch_elf *elf,
                                const struct livepatch_elf_sec *base,
                                const struct livepatch_elf_sec *rela)
{
    unsigned int i;

    for ( i = 0; i < (rela->sec->sh_size / rela->sec->sh_entsize); i++ )
    {
        const Elf_RelA *r = rela->data + i * rela->sec->sh_entsize;
        unsigned int symndx = ELF64_R_SYM(r->r_info);
        uint8_t *dest = base->load_addr + r->r_offset;
        uint64_t val;

        if ( symndx == STN_UNDEF )
        {
            printk(XENLOG_ERR LIVEPATCH "%s: Encountered STN_UNDEF\n",
                   elf->name);
            return -EOPNOTSUPP;
        }
        else if ( symndx >= elf->nsym )
        {
            printk(XENLOG_ERR LIVEPATCH "%s: Relative relocation wants symbol@%u which is past end\n",
                   elf->name, symndx);
            return -EINVAL;
        }
        else if ( !elf->sym[symndx].sym )
        {
            printk(XENLOG_ERR LIVEPATCH "%s: No symbol@%u\n",
                   elf->name, symndx);
            return -EINVAL;
        }

        val = r->r_addend + elf->sym[symndx].sym->st_value;

        switch ( ELF64_R_TYPE(r->r_info) )
        {
        case R_X86_64_NONE:
            break;

        case R_X86_64_64:
            if ( r->r_offset >= base->sec->sh_size ||
                (r->r_offset + sizeof(uint64_t)) > base->sec->sh_size )
                goto bad_offset;

            *(uint64_t *)dest = val;
            break;

        case R_X86_64_PLT32:
            /*
             * Xen uses -fpic which normally uses PLT relocations
             * except that it sets visibility to hidden which means
             * that they are not used.  However, when gcc cannot
             * inline memcpy it emits memcpy with default visibility
             * which then creates a PLT relocation.  It can just be
             * treated the same as R_X86_64_PC32.
             */
        case R_X86_64_PC32:
            if ( r->r_offset >= base->sec->sh_size ||
                (r->r_offset + sizeof(uint32_t)) > base->sec->sh_size )
                goto bad_offset;

            val -= (uint64_t)dest;
            *(int32_t *)dest = val;
            if ( (int64_t)val != *(int32_t *)dest )
            {
                printk(XENLOG_ERR LIVEPATCH "%s: Overflow in relocation %u in %s for %s\n",
                       elf->name, i, rela->name, base->name);
                return -EOVERFLOW;
            }
            break;

        default:
            printk(XENLOG_ERR LIVEPATCH "%s: Unhandled relocation %lu\n",
                   elf->name, ELF64_R_TYPE(r->r_info));
            return -EOPNOTSUPP;
        }
    }

    return 0;

 bad_offset:
    printk(XENLOG_ERR LIVEPATCH "%s: Relative relocation offset is past %s section\n",
           elf->name, base->name);
    return -EINVAL;
}

/*
 * Once the resolving symbols, performing relocations, etc is complete
 * we secure the memory by putting in the proper page table attributes
 * for the desired type.
 */
int arch_livepatch_secure(const void *va, unsigned int pages, enum va_type type)
{
    unsigned long start = (unsigned long)va;
    unsigned int flag;

    ASSERT(va);
    ASSERT(pages);

    if ( type == LIVEPATCH_VA_RX )
        flag = PAGE_HYPERVISOR_RX;
    else if ( type == LIVEPATCH_VA_RW )
        flag = PAGE_HYPERVISOR_RW;
    else
        flag = PAGE_HYPERVISOR_RO;

    return modify_xen_mappings(start, start + pages * PAGE_SIZE, flag);
}

void __init arch_livepatch_init(void)
{
    void *start, *end;

    start = (void *)__2M_rwdata_end;
    end = (void *)(XEN_VIRT_END - FIXADDR_X_SIZE - NR_CPUS * PAGE_SIZE);

    BUG_ON(end <= start);

    vm_init_type(VMAP_XEN, start, end);
}
/*
 * Local variables:
 * mode: C
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
