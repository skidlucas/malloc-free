/*
 * my-malloc.c 	-- Implementation de malloc, free, calloc, realloc
 *
 * Implémentation first-fit pour malloc
 *
 *           Author: Lucas Martinez
 *    		 Version: 26/06/2015
 */


#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include "my-malloc.h"

#define HEADER_SIZE sizeof(Header)
#define BLOCK_SIZE 1008 //comme ça BLOCK_SIZE + HEADER_SIZE = 1024
#define MOST_RESTRICTING_TYPE double // Pour s’aligner sur des frontieres multiples
									 // de la taille du type le plus contraignant
void *sbrk(intptr_t increment);
static int nb_alloc   = 0;              /* Nombre de fois où on alloué     */
static int nb_dealloc = 0;              /* Nombre de fois où on désalloué  */
static int nb_sbrk    = 0;              /* nombre de fois où a appelé sbrk */
void *base = NULL;

typedef union header { // Header de bloc
	struct {
		unsigned int size; // Taille du bloc
		union header *next; // bloc libre suivant (renommé next)
	} info;
	MOST_RESTRICTING_TYPE dummy; // Ne sert qu’a provoquer un alignement
} Header;

Header *search(Header **last, size_t size){
	Header *current = base;
	while (current && !(current->info.size >= size)) { // tant qu'on a pas trouvé un bloc libre et suffisamment grand
		//(current && !(current->info.is_free && current->info.size >= size)) ATTENTION ON NE PEUT PAS MODIFIER LE HEADER
		*last = current;
		current = current->info.next;
	}
	return current;
}

Header *extend_heap(Header *last, size_t size) {
	Header *block;
	block = sbrk(0);
	void *request = sbrk(size + HEADER_SIZE);
  	//assert((void*)block == request); // Not thread safe.

  	if(request == (void*) -1) {
  		fprintf(stderr, "No more memory\n");
  		return NULL; //sbrk failed
  	}

  	if (last) { //NULL la première fois
  		last->info.next = block;
  	}	

  	block->info.size = size;
  	block->info.next = NULL;
  	//block->info.is_free = 0;		ATTENTION ON PEUT PAS MODIFIER LE HEADER

  	++nb_sbrk;
  	
  	return block;
}

void split(Header *block, size_t size){
	Header *new_block;
	new_block = block + HEADER_SIZE;
	new_block->info.size = block->info.size - (size + HEADER_SIZE);
	new_block->info.next = block->info.next;
	block->info.size = size;
	block->info.next = new_block;
}

void *mymalloc(size_t size) {
	Header *block;

	if (size <= 0) {
		return NULL;
	}

	if (!base) { // premier appel.
		block = extend_heap(NULL, BLOCK_SIZE);
		if (!block) {
		  	return NULL;
		}
		base = block;
	} else {
		Header *last = base;
		block = search(&last, size);
		if (!block) { // on n'a pas trouvé de bloc libre
		  	block = extend_heap(last, BLOCK_SIZE); //alors on agrandit le tas
		  	if (!block) {
		    	return NULL; //NULL si extend_heap n'a pas marché
		  	}
		} else { //on a trouvé un bloc libre
		  	if ((block->info.size) >= (HEADER_SIZE + 4)){ //si la taille du bloc est plus grande qu'un header + 4 octets (bloc minimal)
				split(block, block->info.size); //alors on split //TODO: verif size
			}
		  	//block->info.is_free = 0; //le block n'est plus free ATTENTION ON NE PEUT PAS MODIFIER LE HEADER
		}
	}

	++nb_alloc;
	return(block + 1); // +1 car on renvoit le pointeur sur la région après notre bloc (et block + 1 incrémente d'un HEADER_SIZE)
}

void myfree(void *p) {

}

int main(int argc, char const *argv[])
{
	printf("%lu\n", HEADER_SIZE);
	printf("%d\n", nb_alloc);
	printf("%d\n", nb_sbrk);
	return 0;
}