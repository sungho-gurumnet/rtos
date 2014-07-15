#include <string.h>
#include <util/event.h>
#include "asm.h"
#include "apic.h"
#include "mp.h"
#include "shared.h"
#include "gmalloc.h"
#include "task.h"

#include "icc.h"

static ICC_Message* icc_msg;

#define ICC_EVENTS_COUNT	64
typedef void (*ICC_Handler)(ICC_Message*);
static ICC_Handler icc_events[ICC_EVENTS_COUNT];

static bool is_event;

static void icc(uint64_t vector, uint64_t err) {
	icc_msg->status = ICC_STATUS_RECEIVED;
	
	if(icc_events[icc_msg->type])
		is_event = true;
	else
		icc_msg->status = ICC_STATUS_DONE;
	
	apic_eoi();
}

/*
static void terminate(uint64_t vector, uint64_t err) {
	icc_msg->status = ICC_STATUS_DONE;
	apic_eoi();
	
	task_destroy(1);
}
*/

static bool icc_event(void* context) {
	if(is_event) {
		icc_events[icc_msg->type](icc_msg);
		is_event = false;
	}
	
	return true;
}

void icc_init() {
	if(mp_core_id() == 0) {
		uint8_t core_count = mp_core_count();
		
		shared->icc_messages = gmalloc(sizeof(ICC_Message) * core_count);
		bzero(shared->icc_messages, sizeof(ICC_Message) * core_count);
	}
	
	icc_msg = &shared->icc_messages[mp_core_id()];
	
	event_busy_add(icc_event, NULL);
	
	apic_register(48, icc);
	//apic_register(49, terminate);
}

ICC_Message* icc_sending(uint8_t type, uint16_t core_id) {
	while(shared->icc_messages[core_id].status != ICC_STATUS_DONE);
	
	shared->icc_messages[core_id].id++;
	shared->icc_messages[core_id].type = type;
	shared->icc_messages[core_id].core_id = mp_core_id();
	
	return &shared->icc_messages[core_id];
}

uint32_t icc_send(ICC_Message* msg) {
	if(msg->type == ICC_TYPE_STOP) {
		msg->status = ICC_STATUS_SENT;
		uint64_t core_id = msg - shared->icc_messages;
		apic_write64(APIC_REG_ICR, ((uint64_t)core_id << 56) |
					APIC_DSH_NONE | 
					APIC_TM_EDGE | 
					APIC_LV_DEASSERT | 
					APIC_DM_PHYSICAL | 
					APIC_DMODE_FIXED |
					49);
		
		msg->status = ICC_STATUS_DONE;
		
		return msg->id;
	} else {
		msg->status = ICC_STATUS_SENT;
		
		uint64_t core_id = msg - shared->icc_messages;
		apic_write64(APIC_REG_ICR, ((uint64_t)core_id << 56) |
					APIC_DSH_NONE | 
					APIC_TM_EDGE | 
					APIC_LV_DEASSERT | 
					APIC_DM_PHYSICAL | 
					APIC_DMODE_FIXED |
					48);
		
		return msg->id;
	}
}

void icc_register(uint8_t type, void(*event)(ICC_Message*)) {
	icc_events[type] = event;
}
