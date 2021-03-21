#pragma once

#include <stdbool.h>

#define QUEUE_FL_SENDER (1<<0)
#define QUEUE_FL_OWNER  (1<<1)

typedef enum {
    QUEUE_ERR_OK,
    QUEUE_ERR_GENERAL_FAILURE,
    QUEUE_ERR_NOT_SUPPORTED,
    QUEUE_ERR_NOT_OWNER,
} queue_err_t;

typedef enum {
    QUEUE_ATTR_MAX_QUEUE_SIZE,       // in bytes, System V only (even if for POSIX we can get it by: mq_msgsize * mq_maxmsg)
    QUEUE_ATTR_MAX_MESSAGE_SIZE,     // in bytes, POSIX only
    QUEUE_ATTR_MAX_MESSAGE_IN_QUEUE, // POSIX only
} queue_attr_t;

/**
 * Calls order:
 *   queue_init > queue_set_attribute* > queue_open > queue_send or queue_receive > queue_close
 **/

/**
 * Initialize and allocate a queue
 *
 * @return a queue (a valid pointer) or NULL on failure
 **/
void *queue_init(char **);

/**
 * Set an attribute (a maximum size of an internal stuff)
 *
 * Restrictions:
 * - only the owner of the queue can set an attribute
 * - attributes should be set between queue_init and queue_open as various queue internal sizes/lengths can't be
 * altered after queue creation
 *
 * @param queue
 * @param attribute
 * @param value
 *
 * @return
 * - QUEUE_ERR_OK if new value is effective
 * - QUEUE_ERR_NOT_OWNER if "current process" is not the owner of the queue
 * - QUEUE_ERR_NOT_SUPPORTED if given attribute is invalid or not handled by the underlaying implementation
 * - QUEUE_ERR_GENERAL_FAILURE on internal failure
 **/
queue_err_t queue_set_attribute(void *, queue_attr_t, unsigned long);

/**
 * Open the queue to send or receive message
 *
 * @param queue
 * @param filename
 * @param flags, a mask of:
 *   - QUEUE_FL_RECEIVER: to receive messages
 *   - QUEUE_FL_SENDER:   to send messages
 *   - QUEUE_FL_OWNER:    to own the queue (ie take in charge its creation and deletion)
 *
 * @return true on success
 *
 * Note: even if queue_open fails, call queue_close to cleanup internal stuffs
 **/
bool queue_open(void *, const char *, int, char **);

/**
 * Get current value of an attribute (a maximum size of an internal stuff)
 *
 * Note: should only be used after queue_open.
 *
 * @param queue
 * @param attribute
 * @param value
 *
 * @return
 * - QUEUE_ERR_OK if new value is effective
 * - QUEUE_ERR_NOT_SUPPORTED if given attribute is invalid or not handled by the underlaying implementation
 * - QUEUE_ERR_GENERAL_FAILURE on internal failure
 **/
queue_err_t queue_get_attribute(void *, queue_attr_t, unsigned long *);

/**
 * Receive a message (in blocking mode)
 *
 * @param queue
 * @param buffer
 * @param buffer_size
 *
 * @return -1 on failure or the length of the message
 **/
int queue_receive(void *, char *, size_t, char **);

/**
 * Send a message
 *
 * @param queue
 * @param message
 * @param message_len (length, not size, ie this does not include the final \0)
 *
 * @return true on success
 **/
bool queue_send(void *, const char *, int, char **);

/**
 * Close and deallocate the queue
 *
 * @return true on success
 **/
bool queue_close(void **, char **);
