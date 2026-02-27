#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef struct{
	SDL_Window *window;
	SDL_Renderer *renderer;
}sdl_t; 	

typedef struct{
	uint32_t window_height;
	uint32_t window_width;
	uint32_t fg_color;
	uint32_t bg_color;
	uint32_t scale_factor; 	
	bool pixel_outlines;
	uint32_t inst_per_sec;
}config_t;

typedef enum{
	QUIT,
	RUNNING,
	PAUSED,
}emulator_state_t;

typedef struct{
	uint16_t opcode;
	uint16_t NNN;
	uint8_t NN;
	uint8_t N;
	uint8_t X;
	uint8_t Y;
}instruction_t;

typedef struct{
	emulator_state_t state;
	uint8_t ram[4096];
	bool display[64*32];
	uint16_t stack[12];
	uint16_t *stack_ptr;
	uint8_t V[16];
	uint16_t I;
	uint16_t PC;
	uint8_t delay_timer;
	uint8_t sound_timer;
	bool keypad[16];
	const char *rom_name;
	instruction_t inst;
}chip8_t;

bool init_sdl(sdl_t *sdl,const config_t config){	
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){
		SDL_Log("Could not initialize sdl %s\n",SDL_GetError());
		return false;
	}

	sdl->window = SDL_CreateWindow("Chip8 Emulator",SDL_WINDOWPOS_CENTERED,
								   SDL_WINDOWPOS_CENTERED,
								   config.window_width * config.scale_factor,
								   config.window_height * config.scale_factor,
								   0);
	
	if(!sdl->window){
		SDL_Log("Could not create SDL window %s\n",SDL_GetError());
	}

	sdl->renderer = SDL_CreateRenderer(sdl->window,-1,SDL_RENDERER_ACCELERATED);

	if(!sdl->renderer){
		SDL_Log("Could not create SDL renderer %s\n",SDL_GetError());
	}

	return true;
}

bool set_config_from_args(config_t *config,const int argc, char **argv){
	*config = (config_t){
		.window_width = 64,
		.window_height = 32,
		.fg_color = 0xFFFFFFFF,
		.bg_color = 0x000000FF,
		.scale_factor = 20,
		.inst_per_sec = 500
	};

	for(int i=1;i<argc;i++){
		(void)argv[i];
	}

	return true;
}

bool init_chip8(chip8_t *chip8,const char rom_name[]){
	const uint32_t entry_point = 0x200;
	const uint8_t font[] = {
		0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
		0x20, 0x60, 0x20, 0x20, 0x70, // 1
		0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
		0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
		0x90, 0x90, 0xF0, 0x10, 0x10, // 4
		0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
		0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
		0xF0, 0x10, 0x20, 0x40, 0x40, // 7
		0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
		0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
		0xF0, 0x90, 0xF0, 0x90, 0x90, // A
		0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
		0xF0, 0x80, 0x80, 0x80, 0xF0, // C
		0xE0, 0x90, 0x90, 0x90, 0xE0, // D
		0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
		0xF0, 0x80, 0xF0, 0x80, 0x80  // F
	};

	memcpy(&chip8->ram[0],font,sizeof(font));

	FILE *rom = fopen(rom_name,"rb");
	if(!rom){
		SDL_Log("Rom file %s is invalid or does not exist\n",rom_name);
		return false;
	}

	fseek(rom,0,SEEK_END);
	const size_t rom_size = ftell(rom);
	const size_t max_size = sizeof chip8->ram - entry_point;
	rewind(rom);

	if(rom_size>max_size){
		SDL_Log("Rom file %s is too big! Rom size: %zu, Max size allowed: %zu\n",
				rom_name,rom_size,max_size);
		return false;
	}

	if(fread(&chip8->ram[entry_point],rom_size,1,rom) != 1){
		SDL_Log("Could not read Rom file %s into CHIP8 memory\n",rom_name);
		return false;
	}

	fclose(rom);

	chip8->state = RUNNING;
	chip8->PC = entry_point;
	chip8->rom_name = rom_name;
	chip8->stack_ptr = &chip8->stack[0];

	return true;
}

void final_cleanup(const sdl_t sdl){
	SDL_DestroyRenderer(sdl.renderer);
	SDL_DestroyWindow(sdl.window);
	SDL_Quit();
}

void clear_screen(const config_t config,const sdl_t sdl){
	uint8_t r = (config.bg_color >> 24) & 0xFF;
	uint8_t g = (config.bg_color >> 16) & 0xFF;
	uint8_t b = (config.bg_color >> 8) & 0xFF;
	uint8_t a = (config.bg_color >> 0) & 0xFF;

	SDL_SetRenderDrawColor(sdl.renderer,r,g,b,a);
	SDL_RenderClear(sdl.renderer);
}

void update_screen(const sdl_t sdl,const config_t config,const chip8_t chip8){
	SDL_Rect rect = {.x = 0, .y =0, .w = config.scale_factor, .h = config.scale_factor};

	uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
	uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
	uint8_t fg_b = (config.fg_color >> 8) & 0xFF;
	uint8_t fg_a = (config.fg_color >> 0) & 0xFF;

	uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
	uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
	uint8_t bg_b = (config.bg_color >> 8) & 0xFF;
	uint8_t bg_a = (config.bg_color >> 0) & 0xFF;

	for(uint32_t i = 0; i < sizeof chip8.display; i++){

		rect.x = (i%config.window_width) * config.scale_factor;
		rect.y = (i/config.window_width) * config.scale_factor;

		if(chip8.display[i]){
			SDL_SetRenderDrawColor(sdl.renderer,fg_r,fg_g,fg_b,fg_a);
			SDL_RenderFillRect(sdl.renderer,&rect);

			SDL_SetRenderDrawColor(sdl.renderer,bg_r,bg_g,bg_b,bg_a);
			SDL_RenderDrawRect(sdl.renderer,&rect);
		}
		else{
			SDL_SetRenderDrawColor(sdl.renderer,bg_r,bg_g,bg_b,bg_a);
			SDL_RenderFillRect(sdl.renderer,&rect);
		}
	}	

	SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8){
	SDL_Event event;
	
	while(SDL_PollEvent(&event)){
		switch(event.type){
			case SDL_QUIT : 
				chip8->state = QUIT;
				return;

			case SDL_KEYDOWN : 
				switch(event.key.keysym.sym){
					case SDLK_SPACE:
						if(chip8->state == RUNNING){
							chip8->state = PAUSED;
							puts("====PAUSED====");
						} else {
							chip8->state = RUNNING;
						}
						return;
					case SDLK_1 : chip8->keypad[0x1] = true; break;
					case SDLK_2 : chip8->keypad[0x2] = true; break;
					case SDLK_3 : chip8->keypad[0x3] = true; break;
					case SDLK_4 : chip8->keypad[0xC] = true; break;

					case SDLK_q : chip8->keypad[0x4] = true; break;
					case SDLK_w : chip8->keypad[0x5] = true; break;
					case SDLK_e : chip8->keypad[0x6] = true; break;
					case SDLK_r : chip8->keypad[0xD] = true; break;
					
					case SDLK_a : chip8->keypad[0x7] = true; break;
					case SDLK_s : chip8->keypad[0x8] = true; break;
					case SDLK_d : chip8->keypad[0x9] = true; break;
					case SDLK_f : chip8->keypad[0xE] = true; break;

					case SDLK_z : chip8->keypad[0xA] = true; break;
					case SDLK_x : chip8->keypad[0x0] = true; break;
					case SDLK_c : chip8->keypad[0xB] = true; break;
					case SDLK_v : chip8->keypad[0xF] = true; break;
					default : break;
				}
				break;

			case SDL_KEYUP :
				switch(event.key.keysym.sym){
					case SDLK_1 : chip8->keypad[0x1] = false; break;
					case SDLK_2 : chip8->keypad[0x2] = false; break;
					case SDLK_3 : chip8->keypad[0x3] = false; break;
					case SDLK_4 : chip8->keypad[0xC] = false; break;

					case SDLK_q : chip8->keypad[0x4] = false; break;
					case SDLK_w : chip8->keypad[0x5] = false; break;
					case SDLK_e : chip8->keypad[0x6] = false; break;
					case SDLK_r : chip8->keypad[0xD] = false; break;
					
					case SDLK_a : chip8->keypad[0x7] = false; break;
					case SDLK_s : chip8->keypad[0x8] = false; break;
					case SDLK_d : chip8->keypad[0x9] = false; break;
					case SDLK_f : chip8->keypad[0xE] = false; break;

					case SDLK_z : chip8->keypad[0xA] = false; break;
					case SDLK_x : chip8->keypad[0x0] = false; break;
					case SDLK_c : chip8->keypad[0xB] = false; break;
					case SDLK_v : chip8->keypad[0xF] = false; break;
					default : break;
				}
				break;

			default : 
				break;
		}
	}
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8){
	printf("Address: 0x%04X, Opcode: 0x%04X Desc: ",
		  chip8->PC-2,chip8->inst.opcode);

	switch((chip8->inst.opcode>>12) & 0x0F){
		case 0x00 :
			if(chip8->inst.NN == 0xE0){
				//0x00E0 : Clear the screen
				printf("Clear the screen\n");
			}
			else if(chip8->inst.NN == 0xEE){
				//0x00EE : Return from subroutine
				printf("Return from subroutine to address 0x%04X\n",
						*(chip8->stack_ptr - 1));
			}
			else
				printf("unimplemented\n");
			break;
		
		case 0x01:
			//0x1NNN : Jump to address NNN
			printf("Jump to address NNN (0x%04X)\n",chip8->inst.NNN);
			break;

		case 0x02 :
			//0x2NNN : Call subroutine at NNN
			printf("Call subroutine at 0x%04X\n", chip8->inst.NNN);
			break;

		case 0x03 : 
			//0x3XNN : if(VX == NN) skip next instruction
			printf("if V%X (0x%02X) == NN (0x%02X) skip next instruction ",
					chip8->inst.X,chip8->V[chip8->inst.X],chip8->inst.NN);
			break;
		
		case 0x04 : 
			//0x4XNN : if(VX != NN) skip next instruction
			printf("if V%X (0x%02X) != NN (0x%02X) skip next instruction ",
					chip8->inst.X,chip8->V[chip8->inst.X],chip8->inst.NN);
			break;
		
		case 0x05 : 
			//0x5XY0 : if(VX == VY) skip next instruction
			printf("if V%X (0x%02X) == V%X (0x%02X) skip next instruction ",
					chip8->inst.X,chip8->V[chip8->inst.X],chip8->inst.Y,chip8->V[chip8->inst.Y]);
			break;

		case 0x06 :
			//0x6xNN : Set register VX to NN
			printf("Set register V%X to NN (0x%02X)\n", chip8->inst.X, chip8->inst.NN);
			break;

		case 0x07 :
			//0x7xNN : Set register VX += NN
			printf("Set register V%X (0x%02X) += NN (0x%02X). Result : 0x%02X\n",
				    chip8->inst.X,chip8->V[chip8->inst.X], chip8->inst.NN,
					chip8->V[chip8->inst.X] + chip8->inst.NN);
			break;

		case 0x08 :
			switch(chip8->inst.N){
				case 0 :
					//0x8XY0 : Set register VX to VY
					printf("Set register V%X == V%X (0x%02X)\n",
				    		chip8->inst.X,chip8->inst.Y,chip8->V[chip8->inst.Y]);
					break;

				case 1 :
					//0x8XY1 : Set register VX |= VY
					printf("Set register V%X (0x%02X) |= V%X (0x%02X). Result : 0x%02X\n",
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->inst.Y,chip8->V[chip8->inst.Y],
							chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
					break;	
				
				case 2 :
					//0x8XY2 : Set register VX &= VY
					printf("Set register V%X (0x%02X) &= V%X (0x%02X). Result : 0x%02X\n",
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->inst.Y,chip8->V[chip8->inst.Y],
							chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
					break;

				case 3 :
					//0x8XY3 : Set register VX ^= VY
					printf("Set register V%X (0x%02X) ^= V%X (0x%02X). Result : 0x%02X\n",
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->inst.Y,chip8->V[chip8->inst.Y],
							chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
					break;

				case 4 :
					//0x8XY4 : Set register VX += VY and VF = 1 if carry
					printf("Set register V%X (0x%02X) += V%X (0x%02X). Result : 0x%02X, VF = %X\n",
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->inst.Y,chip8->V[chip8->inst.Y],
							chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
							((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255));
					break;
				
				case 5 :
					//0x8XY5 : Set register VX -= VY and VF = 1 if no borrow
					printf("Set register V%X (0x%02X) -= V%X (0x%02X). Result : 0x%02X, VF = %X\n",
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->inst.Y,chip8->V[chip8->inst.Y],
							chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
							(chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y]));
					break;

				case 6 :
					//0x8XY6 : Set register VX >>= 1, store shifted bit in VF
					printf("Set register V%X (0x%02X) >>= 1. Result : 0x%02X, VF = %X\n",
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->V[chip8->inst.X] >> 1,
							chip8->V[chip8->inst.X] & 1);
					break;
				
				case 7 :
					//0x8XY7 : Set register VX = VY - VX and VF = 1 if no borrow
					printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X). Result : 0x%02X, VF = %X\n",
							chip8->inst.X,
							chip8->inst.Y,chip8->V[chip8->inst.Y],
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X],
							(chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
					break;

				case 0xE :
					//0x8XYE : Set register VX <<= 1, store shifted bit in VF
					printf("Set register V%X (0x%02X) <<= 1. Result : 0x%02X, VF = %X\n",
							chip8->inst.X,chip8->V[chip8->inst.X],
							chip8->V[chip8->inst.X] << 1,
							(chip8->V[chip8->inst.X] & 0x80) >> 7);
					break;

				default :
					break;
			}
			break;

		case 0x09 : 
			//0x9XY0 : if VX != VY skip the next instruction
			printf("if V%X (0x%02X) != V%X (0x%02X) skip next instruction ",
					chip8->inst.X,chip8->V[chip8->inst.X],
					chip8->inst.Y,chip8->V[chip8->inst.Y]);
			break;
		
		case 0x0A :
			//0xANNN : Set index register I to NNN
			printf("Set I to NNN (0x%04X)\n",chip8->inst.NNN);
			break;

		case 0x0B :
			//0xBNNN : Jump to V0 + NNN
			printf("Set PC to V0 (0x%02X) + NNN (0x%04X). Result : 0x%04X",
				   chip8->V[0],chip8->inst.NNN,
				   chip8->V[0] + chip8->inst.NNN);
			break;
		
		case 0x0C : 
			//0xCXNN : Setx VX = rand() % 256 & NN
			printf("Setx V%X = rand() %% 256 & NN (0x%02X)\n",chip8->inst.X,chip8->inst.NN);
			break;

		case 0x0D :
			//0xDXYN : Draw N-height sprite at coords X,Y; Read from I
			printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) , Read from I (0x%04X)\n",
					chip8->inst.N, chip8->inst.X,chip8->V[chip8->inst.X],chip8->inst.Y,
					chip8->V[chip8->inst.Y],chip8->I);
			break;

		case 0x0E :
			if(chip8->inst.NN == 0x9E){
				//0xEX9E : Skip next instruction if key in VX is pressed 
				printf("Skip next instruction if key in V%X (0x%02X) is pressed, key : %d\n",
						chip8->inst.X,chip8->V[chip8->inst.X],
						chip8->keypad[chip8->V[chip8->inst.X]]);
			}
			else if(chip8->inst.NN == 0xA1){
				//0xEX9E : Skip next instruction if key in VX is not pressed 
				printf("Skip next instruction if key in V%X (0x%02X) is not pressed, key : %d\n",
						chip8->inst.X,chip8->V[chip8->inst.X],
						chip8->keypad[chip8->V[chip8->inst.X]]);
			}
			break;

		case 0x0F : 
			switch(chip8->inst.NN){
				case 0x0A :
					//0xFX0A : VX = get_key(); Await until a keypress, and store in VX
					printf("Await until a keypress, and store in V%X\n",chip8->inst.X);
					break;

				case 0x1E :
						//0xFX1E : I += VX
						printf("I (0x%04X) += V%X (0x%02X). Result : 0x%04X\n",
								chip8->I,chip8->inst.X,chip8->V[chip8->inst.X],
								chip8->I + chip8->V[chip8->inst.X]);
						break;

				case 0x07 :
						//0xFX07 : VX = delay timer
						printf("V%X = delay timer (0x%02X)\n",chip8->inst.X,chip8->delay_timer);
						break;

				case 0x15 :
						//0xFX15 : delay timer = VX
						printf("delay timer = V%X (0x%02X)\n",chip8->inst.X,chip8->V[chip8->inst.X]);
						break;
				
				case 0x18 :
						//0xFX18 : sound timer = VX
						printf("sound timer = V%X (0x%02X)\n",chip8->inst.X,chip8->V[chip8->inst.X]);;
						break;
				
				case 0x29 :
						//0xFX29 : I = sprite location in VX
						printf("I = sprite location in V%X (0x%02X). Result = (0x%02X)\n",
						chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);
						break;

				case 0x33 :
					//0xFX33 : Store BCD representation of VX at memory offset from I
					printf("Store BCD representation of V%X (0x%02X) at memory offset from I (0x%04X)\n",
							chip8->inst.X, chip8->V[chip8->inst.X],chip8->I);
					break;

				case 0x55 :
					//0xFX55 : Register dumpp V0 - VX inclusive to memory offset from I
					printf("Register dumpp V0 - V%X (0x%02X) inclusive at memory offset from I (0x%04X)\n",
							chip8->inst.X, chip8->V[chip8->inst.X],chip8->I);
					break;

				case 0x65 :
					//0xFX65 : Register load V0 - VX inclusive from memory offset from I
					printf("Register load V0 - V%X (0x%02X) inclusive at memory offset from I (0x%04X)\n",
							chip8->inst.X, chip8->V[chip8->inst.X],chip8->I);
					break;

				default : 
					break;
			}
			break;

		default :
			printf("unimplemented\n");
	}
}
#endif

void emulate_instruction(chip8_t *chip8,const config_t config){

	chip8->inst.opcode = (chip8->ram[chip8->PC]<<8) | chip8->ram[chip8->PC+1];
	chip8->PC += 2;

	chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;	
	chip8->inst.NN = chip8->inst.opcode & 0x0FF;	
	chip8->inst.N = chip8->inst.opcode & 0x0F;	
	chip8->inst.X = (chip8->inst.opcode >>8) & 0x0F;	
	chip8->inst.Y = (chip8->inst.opcode >>4) & 0x0F;

#ifdef DEBUG
	print_debug_info(chip8);
#endif

	switch((chip8->inst.opcode>>12) & 0x0F){
		case 0x00 :
			if(chip8->inst.NN == 0xE0){
				//0x00E0 : Clear the screen
				memset(&chip8->display[0],false,sizeof chip8->display);
			}
			else if(chip8->inst.NN == 0xEE){
				//0x00EE : Return from subroutine
				chip8->PC = *--chip8->stack_ptr;
			}
			break;

		case 0x01:
			//0x1NNN : Jump to address NNN
			chip8->PC = chip8->inst.NNN;
			break;

		case 0x02 :
			//0x2NNN : Call subroutine at NNN
			*chip8->stack_ptr++ = chip8->PC;
			chip8->PC = chip8->inst.NNN;
			break;

		case 0x03 : 
			//0x3XNN : if(VX == NN) skip next instruction
			if(chip8->V[chip8->inst.X] == chip8->inst.NN)
				chip8->PC += 2;
			break;
		
		case 0x04 : 
			//0x4XNN : if(VX != NN) skip next instruction
			if(chip8->V[chip8->inst.X] != chip8->inst.NN)
				chip8->PC += 2;
			break;

		case 0x05 : 
			//0x5XY0 : if(VX == VY) skip next instruction
			if(chip8->inst.N != 0) break;

			if(chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y])
				chip8->PC += 2;
			break;

		case 0x06 :
			//0x6xNN : Set register VX to NN
			chip8->V[chip8->inst.X] = chip8->inst.NN;
			break;
		
		case 0x07 :
			//0x7xNN : Set register VX += NN
			chip8->V[chip8->inst.X] += chip8->inst.NN;
			break;

		case 0x08 :
			switch(chip8->inst.N){
				case 0 :
					//0x8XY0 : Set register VX to VY
					chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
					break;

				case 1 :
					//0x8XY1 : Set register VX |= VY
					chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
					break;	
				
				case 2 :
					//0x8XY2 : Set register VX &= VY
					chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
					break;

				case 3 :
					//0x8XY3 : Set register VX ^= VY
					chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
					break;

				case 4 :
					//0x8XY4 : Set register VX += VY and VF = 1 if carry
					if((uint16_t)(chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y]) > 255)
						chip8->V[0xF] = 1;

					chip8->V[chip8->inst.X] += chip8->V[chip8->inst.Y];
					break;
				
				case 5 :
					//0x8XY5 : Set register VX -= VY and VF = 1 if no borrow
					chip8->V[0xF] = chip8->V[chip8->inst.X] >= chip8->V[chip8->inst.Y];						
					chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
					break;

				case 6 :
					//0x8XY6 : Set register VX >>= 1, store shifted bit in VF
					chip8->V[0xF] = chip8->V[chip8->inst.X] & 1;
					chip8->V[chip8->inst.X] >>= 1;
					break;
				
				case 7 :
					//0x8XY7 : Set register VX = VY - VX and VF = 1 if no borrow
					chip8->V[0xF] = chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y];					
					chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
					break;

				case 0xE :
					//0x8XYE : Set register VX <<= 1, store shifted bit in VF
					chip8->V[0xF] = (chip8->V[chip8->inst.X] & 0x80) >> 7;
					chip8->V[chip8->inst.X] <<= 1;
					break;

				default :
					break;
			}
			break;
		
		case 0x09 : 
			//0x9XY0 : if VX != VY skip the next instruction
			if(chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
				chip8->PC += 2;
			break;

		case 0x0A :
			//0xANNN : Set index register I to NNN
			chip8->I = chip8->inst.NNN;
			break;

		case 0x0B :
			//0xBNNN : Jump to V0 + NNN
			chip8->PC = chip8->V[0] + chip8->inst.NNN;
			break;
		
		case 0x0C : 
			//0xCXNN : Setx VX = rand() % 256 & NN
			chip8->V[chip8->inst.X] = rand() % 256 & chip8->inst.NN;
			break;

		case 0x0D :
			//0xDXYN : Draw N-height sprite at coords X,Y; Read from I	
			uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
			uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;
			const uint8_t orig_X = X_coord;

			chip8->V[0xF] = 0;

			for(uint8_t i = 0; i < chip8->inst.N; i++){
				
				const uint8_t sprite_data = chip8->ram[chip8->I + i];
				X_coord = orig_X;

				for(int8_t j = 7; j >= 0 ; j--){
					
					bool *pixel = &chip8->display[Y_coord*config.window_width + X_coord];
					const bool sprite_bit = (sprite_data & (1 << j));
					
					if(sprite_bit && *pixel){
						chip8->V[0xF] = 1;
					}

					*pixel ^= sprite_bit;

					if(++X_coord >= config.window_width) break;
				}

				if(++Y_coord >= config.window_height) break;
			}
			break;

		case 0x0E :
			if(chip8->inst.NN == 0x9E){
				//0xEX9E : Skip next instruction if key in VX is pressed 
				if(chip8->keypad[chip8->V[chip8->inst.X]])
					chip8->PC += 2;
			}
			else if(chip8->inst.NN == 0xA1){
				//0xEX9E : Skip next instruction if key in VX is not pressed 
				if(!chip8->keypad[chip8->V[chip8->inst.X]])
					chip8->PC += 2;
			}
			break;

		case 0x0F : 
			switch(chip8->inst.NN){
				case 0x0A :
					//0xFX0A : VX = get_key(); Await until a keypress, and store in VX
					bool any_key_pressed = false;
						for(uint8_t i = 0; i < sizeof chip8->keypad; i++){
							if(chip8->keypad[i]){
								chip8->V[chip8->inst.X] = i;
								any_key_pressed = true;
								break;
							}
						}
					if(!any_key_pressed) chip8->PC -= 2;
					break;

				case 0x1E :
						//0xFX1E : I += VX
						chip8->I += chip8->V[chip8->inst.X];
						break;
					
				case 0x07 :
						//0xFX07 : VX = delay timer
						chip8->V[chip8->inst.X] = chip8->delay_timer;
						break;

				case 0x15 :
						//0xFX15 : delay timer = VX
						chip8->delay_timer = chip8->V[chip8->inst.X];
						break;
				
				case 0x18 :
						//0xFX18 : sound timer = VX
						chip8->sound_timer = chip8->V[chip8->inst.X];
						break;

				case 0x29 :
						//0xFX29 : I = sprite location in VX
						chip8->I = chip8->V[chip8->inst.X] * 5;
						break;

				case 0x33 :
					//0xFX33 : Store BCD representation of VX at memory offset from I
					uint8_t BCD = chip8->V[chip8->inst.X];
					chip8->ram[chip8->I + 2] = BCD % 10;
					BCD /= 10; 
					chip8->ram[chip8->I + 1] = BCD % 10;
					BCD /= 10; 
					chip8->ram[chip8->I] = BCD;
					break;
				
				case 0x55 :
					//0xFX55 : Register dumpp V0 - VX inclusive to memory offset from I
					for(uint8_t i = 0; i <= chip8->inst.X; i++)
						chip8->ram[chip8->I + i] = chip8->V[i];
					break;

				case 0x65 :
					//0xFX65 : Register load V0 - VX inclusive from memory offset from I
					for(uint8_t i = 0; i <= chip8->inst.X; i++)
						chip8->V[i] = chip8->ram[chip8->I + i];
					break;	

				default : 
					break;
			}
			break;

		default :
			break;
	}
}

void update_timer(chip8_t *chip8){
	if(chip8->delay_timer > 0)
		chip8->delay_timer--;
	
	if(chip8->sound_timer > 0){
		chip8->sound_timer--; 
	}
}

int main(int argc,char **argv){

	if(argc < 2){
		fprintf(stderr,"Usage: %s <rom_name>\n",argv[0]);
		exit(EXIT_FAILURE);
	}

	config_t config = {0};
	if(!set_config_from_args(&config,argc,argv)) exit(EXIT_FAILURE);

	sdl_t sdl = {0};
	if(!init_sdl(&sdl,config)) exit(EXIT_FAILURE);

	chip8_t chip8 = {0};
	const char *rom_name = argv[1];
	if(!init_chip8(&chip8,rom_name)) exit(EXIT_FAILURE);

	clear_screen(config,sdl);

	srand(time(NULL));
	
	while(chip8.state != QUIT){

		handle_input(&chip8);

		if(chip8.state == PAUSED) continue;

		const uint64_t start_frame_time = SDL_GetPerformanceCounter();

		for(uint32_t i=0;i<config.inst_per_sec/60;i++)
			emulate_instruction(&chip8,config);

		const uint64_t end_frame_time = SDL_GetPerformanceCounter();

		const double time_elapsed = (double)((end_frame_time-start_frame_time)/1000)/SDL_GetPerformanceFrequency();

		SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed : 0);

		update_screen(sdl,config,chip8);
		update_timer(&chip8);
	}

	final_cleanup(sdl);

	exit(EXIT_SUCCESS);

}
