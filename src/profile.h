
#ifndef _PROFILE_H
#define _PROFILE_H

void load_settings(void);
void save_settings(void);

/* graphic profile */

struct Profile {
	int font_use_system; /* Use system font for terminal */
	char font[128];
	char bg_color[64];
	char fg_color[64];
	double alpha;
	int cursor_shape;
	int cursor_blinking;
};

int load_profile(struct Profile *pf, char *filename);
int save_profile(struct Profile *pf, char *filename);
void profile_create_default(struct Profile *pf);

#endif
