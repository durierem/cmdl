# Command Launcher

Le but de ce projet est de réaliser un lanceur de commandes. Le projet est
constitué de trois parties :

1. Une bibliothèque proposant l’implémentation d’une file synchronisée.

2. Le programme « lanceur de commandes » qui utilisera une file synchronisée
afin de récupérer les commandes qu’il doit lancer.

3. Un programme « client » qui utilisera la file synchronisée du lanceur pour y
déposer des demandes de lancement de commandes.

## Manuel d'utilisation

Le lanceur de commandes est constitué d'un programme daemon et d'un programme
client.

Le daemon peut être lancé avec la commande :

```
$ ./cmdld start
```

Les clients peuvent maintenant envoyer leur(s) commande(s) avec :

```
$ ./cmdl <command>
```

Le daemon peut-être arrêté avec la commande :

```
$ ./cmdld stop
```
