/**
 * @brief   Helper functions to create activities
 * @file    activity.c
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

#define HAVE_MQUEUE_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>
#include "log.h"
#include "activity.h"

#define UNKNOWN_SENDER_DESCRIPTOR \
	&(ActivityDescriptor) { \
		.name = "<Unknown sender>" \
	}

#define NULL_FILE_DESCRIPTOR -999

static char *createMessageQueueId(char *activityName) {
	if (!activityName) {
		logErr("["__FILE__"] null pointer at createMessageQueueId(activityName)!");

		return 0;
	}

	size_t idLength = strlen(activityName) + 2;
	char *id = (char *) malloc(idLength);
	memset(id, 0, idLength);
	id[0] = '/';
	strcat(id, activityName);

	return id;
}

#ifdef __XENO__
#define mq_open __real_mq_open
#endif

static mqd_t createMessageQueue(char *activityName, MessageQueueMode messageQueueMode) {
	char *id = createMessageQueueId(activityName);

	if (mq_unlink(id) < 0) {
		// Ignore any errors
	}

	struct mq_attr attributes = {
			.mq_maxmsg = 10,
			.mq_msgsize = MAX_MESSAGE_LENGTH
	};

	mqd_t queue;
	if (messageQueueMode == messageQueue_nonBlocking) {
		queue = mq_open(id, O_CREAT | O_RDONLY | O_NONBLOCK, S_IRWXU | S_IRWXG, &attributes);
	} else {
		queue = mq_open(id, O_CREAT | O_RDONLY, S_IRWXU | S_IRWXG, &attributes);
	}

	free(id);

	if (queue < 0) {
		logErr("[%s] Error creating message queue %s: %s", activityName, id, strerror(errno));
	}

	return queue;
}

static void * runThread(void *argument) {
	Activity *activity = (Activity *)argument;

	logInfo("[%s] Launching...", activity->descriptor->name);

	activity->descriptor->setUp(activity);
	pthread_cleanup_push(activity->descriptor->tearDown, NULL);

	activity->descriptor->run(activity);

	logInfo("[%s] Terminated.", activity->descriptor->name);

	pthread_cleanup_pop(1);

	return NULL;
}

Activity *createActivity(ActivityDescriptor descriptor, MessageQueueMode messageQueueMode) {
	// Create new activity instance
	Activity *activity = (Activity *) malloc(sizeof(Activity));
	memset(activity, 0, sizeof(Activity));

	// Copy activity descriptor (from stack to heap)
	ActivityDescriptor *descriptorCopy = (ActivityDescriptor *)malloc(sizeof(ActivityDescriptor));
	memcpy(descriptorCopy, &descriptor, sizeof(ActivityDescriptor));
	activity->descriptor = descriptorCopy;

	if (descriptor.scope == activityScope_local) {
		// Create new message queue (= queue for incoming messages)
		mqd_t messageQueue = createMessageQueue(descriptor.name, messageQueueMode);
		if (messageQueue < 0) {
			logErr("[%s] Error creating activity's message queue for incoming messages: %s", descriptor.name, strerror(errno));

			// TODO Error handling
		}
		activity->messageQueue = messageQueue;
		activity->messageQueueMode = messageQueueMode;

		activity->polling = NULL_FILE_DESCRIPTOR;

		// Start new thread
		pthread_t thread;
		if (pthread_create(&thread, NULL, runThread, activity) < 0) {
			logErr("[%s] Error creating new thread for activity: %s", descriptor.name, strerror(errno));
			// TODO Error handling
		}
		activity->thread = thread;
	}

	return activity;
}

#ifdef __XENO__
#define mq_close __real_mq_close
#define mq_unlink __real_mq_unlink
#endif

void destroyActivity(Activity *activity) {
	pthread_cancel(activity->thread);
	pthread_join(activity->thread, NULL);

	if (activity->polling != NULL_FILE_DESCRIPTOR) {
		close(activity->polling);
		activity->polling = NULL_FILE_DESCRIPTOR;
	}

	char *messageQueueId = createMessageQueueId(activity->descriptor->name);
	if (mq_close(activity->messageQueue) < 0) {
		logErr("[%s] Error closing activity's message queue for incoming messages: %s", activity->descriptor->name, strerror(errno));
	}
	if (mq_unlink(messageQueueId) < 0) {
		logErr("[%s] Error destroying activity's message queue %s for incoming messages: %d: %s", activity->descriptor->name, messageQueueId, errno, strerror(errno));
	}
	free(messageQueueId);

	free(activity->descriptor);
	free(activity);
}

int waitForEvent(Activity *activity, char *buffer, unsigned long length, unsigned int timeout) {
	return waitForEvent2(activity, NULL, buffer, length, timeout);
}

int waitForEvent2(Activity *activity, ActivityDescriptor *senderDescriptor, void *buffer, unsigned long length, unsigned int timeout) {
	//logInfo("[%s] Going to wait for an event...", activity->descriptor->name);

	//int polling;
	if (activity->polling == NULL_FILE_DESCRIPTOR) {
		if ((/* polling */ activity->polling = epoll_create(1)) < 0) {
			logErr("[%s] Error setting up event waiting: %s", activity->descriptor->name, strerror(errno));

			close(activity->polling);
			activity->polling = NULL_FILE_DESCRIPTOR;

			return -EFAULT;
		}

		struct epoll_event messageQueueEventDescriptor = {
				.events = EPOLLIN,
				.data.fd = activity->messageQueue
		};
		if (epoll_ctl(/* polling */ activity->polling, EPOLL_CTL_ADD, messageQueueEventDescriptor.data.fd, &messageQueueEventDescriptor) < 0) {
			logErr("[%s] Error registering message queue event source: %s", activity->descriptor->name, strerror(errno));

			//close(polling);
			close(activity->polling);
			activity->polling = NULL_FILE_DESCRIPTOR;

			return -EFAULT;
		}
	}
	int polling = activity->polling;

	struct epoll_event firedEvents[1];
	int numberOfFiredEvents;
	numberOfFiredEvents = epoll_wait(polling, firedEvents, 1, timeout);

	//close(polling);

	if (numberOfFiredEvents < 0) {
		logErr("[%s] Error waiting for event: %s", activity->descriptor->name, strerror(errno));

		return -EFAULT;
	} else if (numberOfFiredEvents == 1) {
		//logInfo("[%s] Message received!", activity->descriptor->name);

		unsigned long incomingMessageLength = receiveMessage2(activity, senderDescriptor, buffer, length);

		return incomingMessageLength;
	} else {
		//logInfo("[%s] Timeout occured!", activity->descriptor->name);

		return 0;
	}
}

#ifdef __XENO__
#define mq_receive __real_mq_receive
#endif

int receiveMessage(void *_receiver, char *buffer, unsigned long length) {
	return receiveMessage2(_receiver, NULL, buffer, length);
}

//int receiveMessage2(void *_receiver, char *senderName, char *buffer, unsigned long length) {
int receiveMessage2(void *_receiver, ActivityDescriptor *senderDescriptor, void *buffer, unsigned long length) {
	if (!_receiver) {
		logErr("["__FILE__"] null pointer at receiveMessage(_receiver, ...)!");

		return -EFAULT;
	}

	int result = 0;

	Activity *receiver = (Activity *)_receiver;

	//logInfo("[%s] Going to receive message...", receiver->descriptor->name);

	//char receiveBuffer[MAX_MESSAGE_LENGTH + 1];
	void *receiveBuffer = NULL;
	if (!(receiveBuffer = malloc(MAX_MESSAGE_LENGTH + 1))) {
		logErr("[%s] Error allocating receiver buffer: %s", receiver->descriptor->name, strerror(errno));

		return -EFAULT;
	}

	ssize_t receiveLength;
	if ((receiveLength = mq_receive(receiver->messageQueue, receiveBuffer, /* sizeof(receiveBuffer) */ MAX_MESSAGE_LENGTH + 1, NULL)) < 0) {
		if (receiver->messageQueueMode != messageQueue_nonBlocking) {
			logErr("[%s] Error receiving message: %s", receiver->descriptor->name, strerror(errno));

			result = -EFAULT;

			goto receiveMessage2_out;
		}
	}

	// Copy message length
	unsigned long messageLength;
	memcpy(&messageLength, receiveBuffer, sizeof(unsigned long));

	// Check length of the receive message
	if (messageLength > length) {
		logErr("[%s] Error receiving message: Message longer than expected!", receiver->descriptor->name);

		result = -EFAULT;

		goto receiveMessage2_out;
	}

	// Copy message
	memcpy(buffer, receiveBuffer + sizeof(unsigned long), messageLength);
	result = messageLength;

	//char *_senderName = NULL;
	ActivityDescriptor *_senderDescriptor = NULL;
	if (receiveLength > sizeof(unsigned long) + messageLength) {
		//_senderName = receiveBuffer + length;
		_senderDescriptor = receiveBuffer + sizeof(unsigned long) + messageLength;
	}

	//if (senderName) {
	if (senderDescriptor) {
		// Copy sender name
		//if (_senderName) {
		if (_senderDescriptor) {
			//memcpy(senderName, _senderName, MAX_ACTIVITY_NAME_LENGTH);
			memcpy(senderDescriptor, _senderDescriptor, sizeof(ActivityDescriptor));
		} else {
			//senderName[0] = '\0';
			//memcpy(senderName, UNKNOWN_SENDER_NAME, strlen(UNKNOWN_SENDER_NAME) + 1);
			memcpy(senderDescriptor, UNKNOWN_SENDER_DESCRIPTOR, sizeof(ActivityDescriptor));
		}
	}

	//if (_senderName) {
	if (_senderDescriptor) {
		//logInfo("[%s] Message received from %s (message length: %u)...", receiver->descriptor->name, _senderName, length);
		//logInfo("[%s] Message received from %s (message length: %u)...", receiver->descriptor->name, _senderDescriptor->name, length);
	} else {
		//logInfo("[%s] Message received (message length: %u)...", receiver->descriptor->name, messageLength);
	}

receiveMessage2_out:
	free(receiveBuffer);

	return result;
}

int sendMessage(ActivityDescriptor receiverDescriptor, char *buffer, unsigned long length, MessagePriority priority) {
	return sendMessage2(NULL, receiverDescriptor, length, buffer, priority);
}

int sendMessage2(void *_sender, ActivityDescriptor receiverDescriptor, unsigned long length, void *buffer, MessagePriority priority) {
//	if (!_sender) {
//		logErr("["__FILE__"] null pointer at senderMessage(_sender, ...)!");
//
//		return -EFAULT;
//	}

	if (strcmp(receiverDescriptor.name, "<Null activity>") == 0) {
		return 0;
	}

	int result = 0;

	Activity *sender = (Activity *)_sender;

	unsigned long sendLength;

	if (sender) {
		sendLength = sizeof(ActivityDescriptor) + sizeof(length) + length;
	} else {
		sendLength = sizeof(length) + length;
	}

	if (sendLength > MAX_MESSAGE_LENGTH) {
		logErr("[%s] Error sending message: Message too long!", "<Sender>");

		return -EFAULT;
	}

	char *receiverMessageQueueId = createMessageQueueId(receiverDescriptor.name);

	mqd_t receiverQueue = mq_open(receiverMessageQueueId, O_WRONLY);
	free(receiverMessageQueueId);
	if (receiverQueue < 0) {
		// special message if display is not running:
		if (errno == ENOENT) {
			logWarn("[%s] %s is not running!", sender->descriptor->name, receiverDescriptor.name);
		} else {
			if (sender) {
				logErr("[%s] Error opening message queue %s for sending: %s", sender->descriptor->name, receiverDescriptor.name, strerror(errno));
			} else {
				logErr("[%s] Error opening message queue %s for sending: %s", "<Sender>", receiverDescriptor.name, strerror(errno));
			}
		}

		return -EFAULT;
	}

	if (sender) {
		//logInfo("[%s] Sending message to %s (message length: %u)...", sender->descriptor->name, receiverDescriptor.name, length);
	} else {
		//logInfo("[%s] Sending message to %s (message length: %u)...", "<Sender>", receiverDescriptor.name, length);
	}

	void *sendBuffer = 0;
	//if (!(sendBuffer = malloc(MAX_ACTIVITY_NAME_LENGTH + length))) {
	if (!(sendBuffer = malloc(sendLength))) {
		logErr("[%s] Error allocating send buffer: %s", sender->descriptor->name, strerror(errno));

		result = -EFAULT;

		goto sendMessage2_out;
	}

	// Copy message length
	memcpy(sendBuffer, &length, sizeof(length));
	// Copy message
	memcpy(sendBuffer + sizeof(length), buffer, length);

	if (sender) {
		// Copy sender name
		//memcpy(sendBuffer + length, sender->descriptor->name, MAX_ACTIVITY_NAME_LENGTH);
		// copy sender descriptor
		memcpy(sendBuffer + sizeof(length) + length, sender->descriptor, sizeof(ActivityDescriptor));
		//if (mq_send(receiverQueue, sendBuffer, MAX_ACTIVITY_NAME_LENGTH + length, priority) < 0) {
		if (mq_send(receiverQueue, sendBuffer, sendLength, priority) < 0) {
			logErr("[%s] Error sending message: %s", sender->descriptor->name, strerror(errno));

			result = -EFAULT;

			goto sendMessage2_out;
		}
	} else {
		if (mq_send(receiverQueue, sendBuffer, sendLength, priority) < 0) {
			logErr("[%s] Error sending message: %s", "<Sender>", strerror(errno));

			result = -EFAULT;

			goto sendMessage2_out;
		}
	}

sendMessage2_out:
	free(sendBuffer);

	mq_close(receiverQueue);

	return result;
}
