/***************************************************************
 * This confidential and  proprietary  software may be used only
 * as authorised  by  a licensing  agreement  from  ARM  Limited
 *
 *             (C) COPYRIGHT 2013-2014 ARM Limited
 *                      ALL RIGHTS RESERVED
 *
 *  The entire notice above must be reproduced on all authorised
 *  copies and copies  may only be made to the  extent permitted
 *  by a licensing agreement from ARM Limited.
 *
 ***************************************************************/
#include <uvisor.h>
#include <vmpu.h>
#include <halt.h>
#include <debug.h>
#include <memory_map.h>
#include "vmpu_freescale_k64_aips.h"
#include "vmpu_freescale_k64_mem.h"

static void vmpu_fault_memmanage(void)
{
    DEBUG_FAULT(FAULT_MEMMANAGE);
    halt_led(FAULT_MEMMANAGE);
}

static void vmpu_fault_bus(void)
{
    DEBUG_FAULT(FAULT_BUS);

    /* the Freescale MPU results in bus faults when an access is forbidden;
     * hence, a different error pattern is used depending on the case */
    /* Note: since we are halting execution we don't bother clearing the SPERR
     *       bit in the MPU->CESR register */
    if(MPU->CESR >> 27)
        halt_led(NOT_ALLOWED);
    else
        halt_led(FAULT_BUS);
}

static void vmpu_fault_usage(void)
{
    DEBUG_FAULT(FAULT_USAGE);
    halt_led(FAULT_USAGE);
}

static void vmpu_fault_hard(void)
{
    DEBUG_FAULT(FAULT_HARD);
    halt_led(FAULT_HARD);
}

static void vmpu_fault_debug(void)
{
    DEBUG_FAULT(FAULT_DEBUG);
    halt_led(FAULT_DEBUG);
}

void vmpu_acl_add(uint8_t box_id, void* start, uint32_t size, UvisorBoxAcl acl)
{
    int res;

#ifndef NDEBUG
    const MemMap *map;
#endif/*NDEBUG*/

    /* check for maximum box ID */
    if(box_id>=UVISOR_MAX_BOXES)
        HALT_ERROR(SANITY_CHECK_FAILED, "box ID out of range (%i)\n", box_id);

    /* check for alignment to 32 bytes */
    if(((uint32_t)start) & 0x1F)
        HALT_ERROR(SANITY_CHECK_FAILED, "ACL start address is not aligned [0x%08X]\n", start);

    /* round ACLs if needed */
    if(acl & UVISOR_TACL_SIZE_ROUND_DOWN)
        size = UVISOR_ROUND32_DOWN(size);
    else
        if(acl & UVISOR_TACL_SIZE_ROUND_UP)
            size = UVISOR_ROUND32_UP(size);

    DPRINTF("\t@0x%08X size=%06i acl=0x%04X [%s]\n", start, size, acl,
        ((map = memory_map_name((uint32_t)start))!=NULL) ? map->name : "unknown"
    );

    /* check for peripheral memory, proceed with general memory */
    if(acl & UVISOR_TACL_PERIPHERAL)
        res = vmpu_aips_add(box_id, start, size, acl);
    else
        res = vmpu_mem_add(box_id, start, size, acl);

    if(!res)
        HALT_ERROR(NOT_ALLOWED, "ACL in unhandled memory area\n");
    else
        if(res<0)
            HALT_ERROR(SANITY_CHECK_FAILED, "ACL sanity check failed [%i]\n", res);
}

int vmpu_switch(uint8_t src_box, uint8_t dst_box)
{
    /* switch ACLs for peripherals */
    vmpu_aips_switch(src_box, dst_box);

    /* switch ACLs for memory regions */
    vmpu_mem_switch(src_box, dst_box);

    return 0;
}

void vmpu_init_protection(void)
{
    /* setup security "bluescreen" exceptions */
    ISR_SET(MemoryManagement_IRQn, &vmpu_fault_memmanage);
    ISR_SET(BusFault_IRQn,         &vmpu_fault_bus);
    ISR_SET(UsageFault_IRQn,       &vmpu_fault_usage);
    ISR_SET(HardFault_IRQn,        &vmpu_fault_hard);
    ISR_SET(DebugMonitor_IRQn,     &vmpu_fault_debug);

    /* enable mem, bus and usage faults */
    SCB->SHCSR |= 0x70000;

    /* init memory protection */
    vmpu_mem_init();

    /* load ACLs for box 0 */
    vmpu_aips_switch(0, 0);
}