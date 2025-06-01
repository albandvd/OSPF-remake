# OSPF-remake

## Mode test 

Pour tester l'infrastructure lancer la commande suivante à la racine du projet: 

``` sh 
docker-compose build
docker-compose up
```

Assurez vous d'avoir mis à jour le Makefile pour que les modifications soient bien prise en compte 

## Mode Dev 

Pour lancer un contnaire Alpine pour pouvoir réaliser des test de dev lancer la commande suivante:

``` sh
cd dev | docker build -t dev-ospf | docker run --name dev -rm -it dev-ospf
```

Assurez vous d'avoir mis à jour le Makefile pour que les modifications soient bien prise en compte 