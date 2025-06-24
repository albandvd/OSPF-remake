# OSPF-remake

## Mode test 

Pour tester l'infrastructure lancer la commande suivante à la racine du projet: 

``` sh 
docker-compose up -d --build
```

Assurez vous d'avoir mis à jour le Makefile pour que les modifications soient bien prise en compte 

Se connecter à un pod

``` sh
docker exec -it r1 sh
```

## Mode Dev 

Pour lancer un contnaire Alpine pour pouvoir réaliser des test de dev lancer la commande suivante:

``` sh
cd dev
docker build -t dev-ospf ./; docker run --name dev --rm -it --cap-add=NET_ADMIN dev-ospf
```

Assurez vous d'avoir mis à jour le Makefile pour que les modifications soient bien prise en compte 