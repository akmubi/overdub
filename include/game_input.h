#ifndef GAME_INPUT_H
#define GAME_INPUT_H

#include "input.h"
#include "types.h"

/* false when the game binds the key to an action that must happen at once (jump, dodge, attacks...),
 * read from the game's action mappings */
bool
game_input_key_can_wait(input_key_kind_t key);
/* reads the mappings once the engine is up and logs which buttons may wait */
void
game_input_init(void);

#endif /* GAME_INPUT_H */
