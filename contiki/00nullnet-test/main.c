#include<contiki.h>
#include<net/netstack.h>
#include<net/nullnet/nullnet.h>
#include<stdio.h>
#include<string.h>

PROCESS(nullnet_test,"Trying Nullnet");
AUTOSTART_PROCESSES(&nullnet_test);

void callback_function(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest){
    unsigned int count;
    if(len == sizeof(count)){
        memcpy(&count,data,sizeof(count));
        printf("Recieved: %u\n",count);

    }
}

PROCESS_THREAD(nullnet_test,ev,data){
    static struct etimer timer_1;
    static unsigned int counter = 0;

    PROCESS_BEGIN();

    nullnet_buf = (uint8_t *)&counter;
    nullnet_len = sizeof(counter);

    nullnet_set_input_callback(callback_function);

    etimer_set(&timer_1,(0.01*CLOCK_SECOND));

    while(1){
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer_1));
        printf("Sending: %u\n",counter);

        NETSTACK_NETWORK.output(NULL);

        counter++;
        etimer_reset(&timer_1);

    }

    PROCESS_END();
}