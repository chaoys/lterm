
#ifndef _PROTOCOL_H
#define _PROTOCOL_H

#define PROT_FLAG_NO 0
#define PROT_FLAG_ASKUSER 1
#define PROT_FLAG_ASKPASSWORD 2
#define PROT_FLAG_DISCONNECTCLOSE 4
#define PROT_FLAG_MASK 255

struct Protocol {
	char command[256];
	char args[256];
	int port;
	unsigned int flags;
};

#endif
