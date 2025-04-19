#include "fractal.h"

static payload_t *payload_queue;
static int payload_amount;

static response_t *response_queue;
static int response_amount;

// Main thread manages the window and user interaction
void ui_thread () {
  //This action is guided by the user
  //lock - payload_queue
  //payload producer: add to the payload_queue when user selects a new area
  //- each time you generate a new payload, you increase the generation
  //signal
  //unlock
}

void render_thread() {
  //This action is guided by the responses from the coordinator
  //wait
  //lock - response_queue
  //do the rendering (printf de um oi)
  //unlock
}

void net_thread_send_payload ()
{
  //lock - payload_queue
  //payload consumer: take from the payload_queue and
  // send throught the socket
  //unlock
  //wait
}

void net_thread_receive_response ()
{
  //block waiting from the coordinator
  //lock - response_queue
  //add the response to the response_queue
  //signal render_thread
  //unlock
}

int main()
{
  //connect to the server (the coordinator)
  //launch ui_thread
  //launch render_thread
  //launch the net_thread_send_payload
  //launch the net_thread_receive_response

  //join
}
