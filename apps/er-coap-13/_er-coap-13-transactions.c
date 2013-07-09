typedef struct coap_transaction {
    struct coap_transaction* next;
    uint16_t mid;
    etimer retrans_timer;
    uint8_t retrans_counter;
    uip_ipaddr_t addr;
    uint16_t port;
    restful_response_handler callback;
    void* callback_data;
    uint16_t packet_len;
    uint8_t packet [COAP_MAX_PACKET_SIZE + 1];
} coap_transaction_t;
MEMB (transactions_memb, coap_transaction_t, COAP_MAX_OPEN_TRANSACTIONS);
LIST (transactions_list);
process* transaction_handler_process = NULL;
void coap_register_as_transaction_handler () {
    transaction_handler_process = PROCESS_CURRENT ();
}
coap_transaction_t* coap_new_transaction (uint16_t mid, uip_ipaddr_t* addr, uint16_t port) {
    coap_transaction_t* t = memb_alloc (& transactions_memb);
    if (t) {
        t->mid = mid;
        t->retrans_counter = 0;
        uip_ipaddr_copy (& t->addr, addr);
        t->port = port;
        list_add (transactions_list, t);
    }
    return t;
}
void coap_send_transaction (coap_transaction_t* t) {
    coap_send_message (& t->addr, t->port, t->packet, t->packet_len);
    if (COAP_TYPE_CON == ((COAP_HEADER_TYPE_MASK & t->packet [0]) >> COAP_HEADER_TYPE_POSITION)) {
        if (t->retrans_counter < COAP_MAX_RETRANSMIT) {
            if (t->retrans_counter == 0) {
                t->retrans_timer.timer.interval = COAP_RESPONSE_TIMEOUT_TICKS + (random_rand () % (clock_time_t) COAP_RESPONSE_TIMEOUT_BACKOFF_MASK);
            } else {
                t->retrans_timer.timer.interval <<= 1;
            }
            process* process_actual = PROCESS_CURRENT ();
            process_current = transaction_handler_process;
            etimer_restart (& t->retrans_timer);
            process_current = process_actual;
            t = NULL;
        } else {
            restful_response_handler callback = t->callback;
            void* callback_data = t->callback_data;
            coap_clear_transaction (t);
            if (callback) {
                callback (callback_data, NULL);
            }
        }
    } else {
        coap_clear_transaction (t);
    }
}
void coap_clear_transaction (coap_transaction_t* t) {
    if (t) {
        etimer_stop (& t->retrans_timer);
        list_remove (transactions_list, t);
        memb_free (& transactions_memb, t);
    }
}
coap_transaction_t* coap_get_transaction_by_mid (uint16_t mid) {
    coap_transaction_t* t = NULL;
    for (t = (coap_transaction_t*) list_head (transactions_list); t; t = t->next) {
        if (t->mid == mid) {
            return t;
        }
    }
    return NULL;
}
void coap_check_transactions () {
    coap_transaction_t* t = NULL;
    for (t = (coap_transaction_t*) list_head (transactions_list); t; t = t->next) {
        if (etimer_expired (& t->retrans_timer)) {
            ++ (t->retrans_counter);
            coap_send_transaction (t);
        }
    }
}
////////////////////global
    coap_register_as_transaction_handler ();
    while (1) {
        PROCESS_YIELD ();
        if (ev == PROCESS_EVENT_TIMER) {
            coap_check_transactions ();
///////////////////client request
    state->transaction = coap_new_transaction (request->mid, remote_ipaddr, remote_port);
    if (state->transaction) {
        state->transaction->callback = coap_blocking_request_callback;
        state->transaction->callback_data = state;
        request2 (state->transaction, state);
        coap_send_transaction (state->transaction);
        ...

///////////////////server
///////////////////request
    transaction = coap_new_transaction (message->mid, & UIP_IP_BUF->srcipaddr, UIP_UDP_BUF->srcport);
    if (transaction) {
        request1 (transaction, NULL);
        ...

///////////////////response
    if (transaction = coap_get_transaction_by_mid (message->mid)) {
        if (transaction->callback) {
            transaction->callback(transaction->callback_data, message);
        }
        coap_clear_transaction (transaction);
    }
    transaction = NULL;
//////////////////both
    if (coap_error_code == NO_ERROR) {
        if (transaction) {
            coap_send_transaction(transaction);
        }
    }
    else {
        coap_clear_transaction(transaction);
    }
