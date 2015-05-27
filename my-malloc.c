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
#define BLOCK_SIZE 800 	
#define MOST_RESTRICTING_TYPE double // Pour s’aligner sur des frontieres multiples
									 // de la taille du type le plus contraignant

typedef union header { // Header de bloc
	struct {
		unsigned int size; // Taille du bloc
		union header *next; // bloc libre suivant (renommé next)
	} info;
	MOST_RESTRICTING_TYPE dummy; // Ne sert qu’a provoquer un alignement
} Header;

void *sbrk(intptr_t increment);
static Header *base = NULL;				// liste vide pour commencer
static int nb_alloc   = 0;              /* Nombre de fois où on alloué     */
static int nb_dealloc = 0;              /* Nombre de fois où on désalloué  */
static int nb_sbrk    = 0;              /* nombre de fois où a appelé sbrk */

/* Fonction qui retourne un pointeur sur les données à partir du bloc */
int get_ptr(Header *block){
    int addr = (intptr_t)block;
    addr += HEADER_SIZE;
    return addr;
}

/* Fonction qui retourne le bloc contenant les données pointées par ptr */
Header *get_block(void *ptr){
    char *tmp;
    tmp = ptr;
    return (ptr = tmp -= HEADER_SIZE);
}

/* Fonction qui cherche un bloc libre pour pouvoir l'allouer avec mymalloc */
Header *search(Header **last, size_t size){
	Header *current = base;
	while (current && (current->info.size < size)) { // tant qu'on a pas trouvé un bloc libre suffisamment grand
		*last = current;
		current = current->info.next;
	}
	return current;
}

/* Fonction qui sert à incrémenter le tas lorsqu'il n'y a plus de blocs libres */
Header *extend_heap(Header *last, size_t size) {
	Header *block;
	block = sbrk(0);
	void *request = sbrk(size + HEADER_SIZE);

  	if(request == (void*) -1) {
  		fprintf(stderr, "No more memory\n");
  		return NULL; //sbrk failed
  	}

  	if (last) { //NULL la première fois
  		last->info.next = block;
  	}

  	block->info.size = size;
  	block->info.next = NULL;

  	nb_sbrk += 1;

  	//debug
  	printf("---->sbrk @%d, size=%d, next:%d\n", block, block->info.size, block->info.next);

  	
  	return block;
}

/* Fonction qui coupe un bloc en deux lors que le bloc alloué est plus grand que la taille nécessaire */
void split(Header *block, size_t size){
	Header *new_block;

	new_block = get_ptr(block) + size;
	new_block->info.size = block->info.size - (size + HEADER_SIZE);
	new_block->info.next = block->info.next;

	block->info.size = size;
	block->info.next = new_block;

	//debug
	printf("-------split------- \nusedblock @0x%d, size=%d, next->0x%d\n", block, block->info.size, block->info.next);
	printf("freeblock @0x%d, size=%d, next->0x%d\n", new_block, new_block->info.size, new_block->info.next);


}

/* Fonction qui supprime le bloc de la liste des blocs libres */
void remove_block(Header *block) {
    if (block == base) {
    	if (block->info.next) {
    		base = block->info.next;
    	} else {
    		base = NULL;
    	}
        return;
    }

    if (base) {
        Header *new_block = base;
        while (new_block->info.next && new_block->info.next != block) {
            new_block = new_block->info.next;
        }
        
        if(new_block->info.next) {
        	new_block->info.next = new_block->info.next->info.next;
        } else {
        	new_block->info.next = NULL;
        }
    }
}

/* Fonction qui alloue de la mémoire */
void *mymalloc(size_t size) {
	Header *block;
	fprintf(stderr, "///////\nWe ask for mymalloc(%d)\n", size);

	if (size <= 0) {
		return NULL;
	}

	if (!base) { // premier appel
		block = extend_heap(NULL, BLOCK_SIZE);
		if (!block) {
		  	return NULL;
		}

		base = block;
	} 

	//ensuite
	if (base) {
		Header *last = base;
		block = search(&last, size);

		if (!block) { // on n'a pas trouvé de bloc libre
		  	block = extend_heap(last, BLOCK_SIZE); //alors on agrandit le tas
		  	if (!block) {
		    	return NULL; //NULL si extend_heap n'a pas marché
		  	}
		} 

		if (block) { //on a trouvé un bloc libre
		  	if ((block->info.size - size) >= (HEADER_SIZE + 4)){ //si la taille du bloc est plus grande qu'un header + 4 octets (bloc minimal)
				split(block, size); //alors on split
			}
		}
	}

	//debug
	printf("mymalloc in block @0x%d, size=%d, next->0x%d\n", block, block->info.size, block->info.next);

	nb_alloc += 1;
	remove_block(block); //enlève le bloc de la liste des blocs libres
	return get_ptr(block); // car on renvoit le pointeur sur la région après notre bloc (soit block + HEADER_SIZE)
}

/* Fonction qui fusionne deux blocs qui se suivent si possible */
void fusion(Header *block){
	Header *next_block = block->info.next;
	if (next_block) {
		if ((get_ptr(block) + block->info.size) == next_block) {
			block->info.size += HEADER_SIZE + next_block->info.size;
			block->info.next = next_block->info.next;
		}
	}
}

/* Fonction qui libère la mémoire allouée précedemment par mymalloc */
void myfree(void *ptr) {
	nb_dealloc += 1;
	Header *block_ptr = get_block(ptr);

	if (!base) {
		base = block_ptr;
	} else {
		if (block_ptr < base) { // nouvelle base
			block_ptr->info.next = base;
			base = block_ptr;
			fusion(base); //fusion avec le suivant

			//debug
			printf("myfree in block @0x%d, size=%d, next->0x%d\n", block_ptr, block_ptr->info.size, block_ptr->info.next);

			return;
		}

		Header *block = base;
		while (block->info.next && block->info.next < block_ptr) {
			block = block->info.next;
		}

		block_ptr->info.next = block->info.next;
		block->info.next = block_ptr;
		fusion(block_ptr); //fusion avec le suivant
		fusion(block); //fusion avec le précédent
	}
	//debug
	printf("myfree in block @0x%d, size=%d, next->0x%d\n", block_ptr, block_ptr->info.size, block_ptr->info.next);
}

/* Fonction qui change la taille du bloc pointé par ptr par la taille size. Le contenu n'est pas changé si la nouvelle
** taille est plus petite. Si elle est plus grande, la mémoire ne sera pas initialisée. Si ptr est NULL, alors l'appel 
** est équivalent à malloc(size). Si size est égal à 0 et ptr est non NULL, alors l'appel est équivalent à free(ptr) */
void *myrealloc(void *ptr, size_t size) {
	printf("realloc");
	if (!ptr) {
		return mymalloc(size); // ptr == NULL donc realloc doit être équivalent à malloc(size)
	}

	Header *block_ptr = get_block(ptr);
	if (block_ptr->info.size >= size) { //si le bloc est plus grand que la nouvelle taille
		if ((block_ptr->info.size - size) >= (HEADER_SIZE + 4)){ //si la taille du bloc est plus grande qu'un header + 4 octets (bloc minimal)
			split(block_ptr, size); //alors on split
		}
		return ptr;
	}

	//sinon il faut allouer un nouvel espace mémoire, copier les données et libérer l'ancien bloc
	void *new_ptr;
	new_ptr = mymalloc(size);
	if (!new_ptr) {
		return NULL;
	}
	memcpy(new_ptr, ptr, block_ptr->info.size); //copie size octets de ptr vers new_ptr
	myfree(ptr); 
	return new_ptr;
}

/* Fonction qui alloue la mémoire nécessaire pour un tableau de nb_elem éléments de taille elem_size octets, et renvoie un pointeur 
** vers la mémoire allouée. Cette zone est remplie avec des zéros. Si nb_elem ou elem_size vaut 0, calloc() renvoie NULL. */
void *mycalloc(size_t nb_elem, size_t elem_size) {
	if(nb_elem == 0 || elem_size == 0){
		return NULL;
	}

	size_t size = nb_elem * elem_size; // Attention à l'overflow (pas vérifié ici)
	void *ptr = mymalloc(size);
	if (ptr){
		memset(ptr, 0, size); //met les données pointées par ptr à 0, jusqu'à une taille size
	}

	return ptr;
}


#ifdef MALLOC_DBG
void mymalloc_infos(char *msg) {
	if (msg) fprintf(stderr, "**********\n*** %s\n", msg);

	fprintf(stderr, "# allocs = %3d - # deallocs = %3d - # sbrk = %3d\n",
			nb_alloc, nb_dealloc, nb_sbrk);
	/* Ca pourrait être pas mal d'afficher ici les blocs dans la liste libre */
	if (base)
    {
    	fprintf(stderr, "Liste of free blocks:\n");
        for (Header *block = base; block; block = block->info.next)
        {
            fprintf(stderr, "Block @0x%d (size=%d, next->0x%d)\n", block, block->info.size, block->info.next);
        }
    }

	if (msg) fprintf(stderr, "**********\n\n");

}
#endif