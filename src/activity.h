/**
 * <short description>
 *
 * @brief   Helper functions to create activities
 * @file    activity.h
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

#ifndef ACTIVITY_H_
#define ACTIVITY_H_

#include <sys/stat.h>
#include <pthread.h>
#include <mqueue.h>

#define MAX_ACTIVITY_NAME_LENGTH 32

#define MESSAGE_CONTENT_DEFINITION_BEGIN(typeId, name) \
	extern const Byte name##Type; \
	struct name##Content {

#define MESSAGE_CONTENT_DEFINITION_END \
	};

#define MESSAGE_CONTENT_TYPE_MAPPING(name, typeId) \
	const Byte name##Type = typeId;

#define MESSAGE_DEFINITION_BEGIN \
	typedef struct { \
		Byte type; \
		union {

#define MESSAGE_CONTENT(name) \
	struct name##Content name;

#define MESSAGE_DEFINITION_END(name) \
		} content; \
	} name##Message;

#define receiveMessage_BEGIN(activity, receiver) \
	{ \
	ActivityDescriptor senderDescriptor; \
	receiver##Message message; \
	int result = receiveMessage2(activity, &senderDescriptor, &message, sizeof(message));

#define receiveMessage_END \
	}

#define sendMessage_BEGIN(sender, receiver, _content) \
	sendMessage2(sender, get##receiver##Descriptor(), &(receiver##Message) { \
		.type = _content##Type, \
		.content._content = {

#define sendResponse_BEGIN(sender, sender2, _content) \
	sendMessage2(sender, senderDescriptor, &(sender2##Message) { \
		.type = _content##Type, \
		.content._content = {

#define sendMessage_END(receiver) \
		} \
	}, sizeof(receiver##Message), messagePriority_medium);

#define MESSAGE_SELECTOR_BEGIN \
	if (0) {

// Obsolete
#define MESSAGE_SELECTOR(message, activityName) \
	} else if (strcmp(message.activity.name, #activityName) == 0) {

#define MESSAGE_BY_SENDER_SELECTOR(sender) \
	} else if (strcmp(senderDescriptor.name, get##sender##Descriptor().name) == 0) {

#define MESSAGE_BY_TYPE_SELECTOR(message, messageType) \
	} else if (message.type == messageType##Type) {

#define MESSAGE_SELECTOR_ANY \
	} else {

#define MESSAGE_SELECTOR_END \
	}

typedef void (*ActivityRun)(void *activity);

typedef struct {
	unsigned int id;
	char name[MAX_ACTIVITY_NAME_LENGTH];
	ActivityRun setUp;
	ActivityRun run;
	ActivityRun tearDown;
} ActivityDescriptor;

typedef enum {
	messageQueue_blocking = 0,
	messageQueue_nonBlocking
} MessageQueueMode;

typedef struct {
	ActivityDescriptor *descriptor;
	pthread_t thread;
	mqd_t messageQueue;
	MessageQueueMode messageQueueMode;
} Activity;

typedef enum {
	messagePriority_low = 0,
	messagePriority_medium,
	messagePriority_high,
} MessagePriority;

Activity *createActivity(ActivityDescriptor descriptor, MessageQueueMode messageQueueMode);
void destroyActivity(Activity *activity);

int waitForEvent(Activity *activity, char *buffer, unsigned long length, unsigned int timeout);
int receiveMessage(void *_receiver, char *buffer, unsigned long length);
//int receiveMessage2(void *_receiver, char *senderName, char *buffer, unsigned long length);
int receiveMessage2(void *_receiver, ActivityDescriptor *senderDescriptor, void *buffer, unsigned long length);
int sendMessage(ActivityDescriptor activity, char *buffer, unsigned long length, MessagePriority priority);
int sendMessage2(void *_sender, ActivityDescriptor activity, void *buffer, unsigned long length, MessagePriority priority);

#endif /* ACTIVITY_H_ */
