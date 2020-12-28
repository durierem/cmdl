/* Le type opaque SQueue représente une file synchronisée partagée en mémoire.
 * 
 * - La taille des éléments d'une file ainsi que la longueur maximale de cette
 * dernière sont à préciser lors de la création de la file.
 * - Les fonctions sq_enqueue, sq_dequeue, sq_length, sq_apply et sq_dispose
 * sont à utiliser avec des objets SQueue préalablement renvoyés par sq_empty
 * ou sq_create.
 * - Il est de la responsabilité de l'utilisateur d'assurer la cohérence de la
 * file vis-à-vis de la taille des objets enfilés. Ceux-ci doivent tous être de
 * la même taille. Il en va de même pour le tampon passé en paramètre de la
 * fonction sq_dequeue.
 */

#ifndef SQUEUE__H
#define SQUEUE__H

#include <stdbool.h>

/**
 * Type opaque pour la manipulation des files synchronisées.
 */
typedef struct __squeue * SQueue;

/**
 * Créé une nouvelle file synchronisée vide.
 *
 * @arg     shm_name    Le nom unique de l'objet SHM à créer.
 * @arg     size        La taille des objets qui seront stockés dans la file.
 * @arg     max_length  La longueur maximale autorisée de la file.
 * @return              Un nouvel objet SQueue.
 */
extern SQueue sq_empty(const char *shm_name, size_t size, size_t max_length);

/**
 * Ouvre une file synchronisée existante.
 *
 * @arg     shm_name    Le nom de l'objet SHM associée à la file à ouvrir.
 * @return              Un objet SQueue.
 */
extern SQueue sq_open(const char *shm_name);

/**
 * Enfile l'objet pointé par obj dans la file synchronisée sq.
 *
 * L'objet pointée par obj est entièrement copié en mémoire partagée.
 *
 * @arg     sq      La file à utiliser.
 * @arg     obj     Un pointeur vers l'objet à enfiler.
 * @return          0 en cas de succès, -1 sinon.
 */
extern int sq_enqueue(SQueue sq, const void *obj);

/**
 * Défle l'objet pointé par obj de la file synchronisée sq.
 *
 * L'objet pointée par obj est entièrement copié en mémoire partagée.
 *
 * @arg     sq      La file à utiliser.
 * @arg     buf     Un pointeur vers une zone mémoire.
 * @return          0 en cas de succès, -1 sinon.
 */
extern int sq_dequeue(SQueue sq, void *buf);

/**
 * Renvoie la longueur courante de la file sq.
 *
 * @param   sq  La file à utilser.
 * @return      La longueur de la file.
 */
extern ssize_t sq_length(const SQueue sq);

/**
 * Applique la fonction fun sur tous les éléments de la file sq.
 *
 * @param   sq      La file à utiliser.
 * @param   fun     Un pointeur vers la fonction à appliquer.
 * @return          0 en cas de succès, le retour de fun en cas d'erreur.
 */
extern int sq_apply(SQueue sq, int (*fun)(void *));

/**
 * Libère les ressources allouées pour la file pointée par sqp.
 *
 * Le pointeur sqp est fixé à NULL à la fin de l'opération.
 *
 * @param   sqp     Un pointeur vers la file à libérer.
 */
extern void sq_dispose(SQueue *sqp);

#endif
