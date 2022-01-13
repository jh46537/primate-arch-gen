#include "pkt_reassembly.h"
#define TCP_FIN 0
#define TCP_SYN 1
#define TCP_RST 2
#define TCP_FACK 4
#define PROT_UDP 0x11
#define PKT_FORWARD 0
#define PKT_DROP 1
#define PKT_CHECK 2
#define INSERT 1
#define UPDATE 2
#define DELETE 3
#define MALLOC 0
#define LOOKUP 1
#define UPDATE0 2

#pragma primate blue match 1 1
void Output(metadata_t *output);
#pragma primate blue match 1 1
void flow_table_read(metadata_t *meta, fce_t *fte);
#pragma primate blue match 1 1
void flow_table_write(int op, fce_t *fte);
#pragma primate blue match 1 1
void flow_table_write(int op, metadata_t *input);
#pragma primate blue match 1 1
void dymem(int op, uint16_t *ptr, dymem_t *pkt);
#pragma primate blue match 1 1
void dymem(int op, metadata_t *input, uint16_t *ptr);
#pragma primate blue match 1 1
void dymem(int op, uint16_t *ptr, uint16_t *next_ptr);


void pkt_reassembly(metadata_t input) {
    fce_t fte;
    if ((input.tcp_flags == (1 << TCP_FACK)) && (input.len == 0)) {
        input.pkt_flags = PKT_FORWARD;
        Output(&input);
        return;
    } else if (input.prot == PROT_UDP) {
        input.pkt_flags = PKT_CHECK;
        Output(&input);
        return;
    } else {
        if (input.len != 0) input.pkt_flags = PKT_CHECK;
        else input.pkt_flags = PKT_FORWARD;
        flow_table_read(&input, &fte);
        if (fte.ch0_bit_map != 0) { // ch0_bit_map shows which sub-table it hits
            // Flow exists
            if (input.seq == fte.seq) {
                if (fte.slow_cnt > 0) {
                    goto RELEASE;
                } else {
                    // in order packet
                    if (input.tcp_flags & (1 << TCP_FIN) | (input.tcp_flags & (1 << TCP_RST))) {
                        flow_table_write(DELETE, &fte);
                    } else {
                        fte.seq = input.seq + input.len;
                        flow_table_write(UPDATE, &fte);
                    }
                    Output(&input);
                    return;
                }
            } else if (input.seq > fte.seq) {
                // GOTO insert packet
                goto INSERT_PKT;
            } else {
                input.pkt_flags = PKT_DROP;
                Output(&input);
                return;
            }
        } else {
            // Flow doesn't exist, insert
            Output(&input);
            if ((input.tcp_flags & (1 << TCP_FIN) | (input.tcp_flags & (1 << TCP_RST))) == 0) {
                flow_table_write(INSERT, &input);
            }
            return;
        }
    }

    RELEASE: 
        dymem_t pkt;
        pkt.meta = input;
        dymem_t pkt_next;
    RELEASE_LOOP:
        dymem(LOOKUP, &(fte.pointer), &pkt_next);
        if (pkt.meta.seq + pkt.meta.len == pkt_next.meta.seq) {
            Output(&(pkt.meta));
            fte.pointer = pkt_next.next;
            pkt = pkt_next;
            if ((--fte.slow_cnt) > 0)
                goto RELEASE_LOOP;
        }
        // update FT
        if (input.tcp_flags & (1 << TCP_FIN) | (input.tcp_flags & (1 << TCP_RST))) {
            flow_table_write(DELETE, &fte);
        } else {
            fte.seq = pkt.meta.seq + pkt.meta.len;
            flow_table_write(UPDATE, &fte);
        }
        Output(&(pkt.meta));
        return;

    INSERT_PKT:
        uint16_t new_node_ptr;
        dymem(MALLOC, &input, &new_node_ptr);
        int slow_cnt = fte.slow_cnt;
        dymem_t head;
        dymem_t tail;
        dymem(LOOKUP, &(fte.pointer), &head); // lookup tail
        dymem(LOOKUP, &(fte.pointer2), &tail); // lookup tail
        uint16_t node_ptr = fte.pointer;
        if (slow_cnt != 0) {
            if (input.seq > tail.meta.seq + tail.meta.len) {
                fte.pointer2 = new_node_ptr;
                dymem(UPDATE0, &(fte.pointer2), &new_node_ptr);
                goto UPDATE_FT;
            } else if (input.seq + input.len < head.meta.seq) {
                dymem(UPDATE0, &new_node_ptr, &(fte.pointer)); //new_node_ptr -> next = fte.pointer
                fte.pointer = new_node_ptr;
                goto UPDATE_FT;
            } else {
    INSERT_LOOP:
                if (input.seq < head.meta.seq + head.meta.len) {
                    //overlap packet, drop
                    input.pkt_flags = PKT_DROP;
                    Output(&input);
                    return;
                } else {
                    dymem_t next_node;
                    dymem(LOOKUP, &(head.next), &next_node);
                    if ((--slow_cnt) == 0) {
                        // insert to tail
                        dymem(UPDATE0, &node_ptr, &new_node_ptr);
                        fte.pointer2 = new_node_ptr;
                        goto UPDATE_FT;
                    } else if (input.seq + input.len > next_node.meta.seq) {
                        node_ptr = head.next;
                        head = next_node;
                        goto INSERT_LOOP;
                    } else {
                        // insert
                        dymem(UPDATE0, &node_ptr, &new_node_ptr);
                        dymem(UPDATE0, &new_node_ptr, &(head.next));
                        goto UPDATE_FT;
                    }
                }
            }
        }
    UPDATE_FT:
        fte.slow_cnt ++;
        flow_table_write(UPDATE, &fte);
        return;
}

