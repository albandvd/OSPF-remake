#Cahier des charges
## 1 Contexte
OSPF   (Open   Shortest   Path   First)   est   un   protocole   de   routage   dynamique   utilisé   pour  
modifier   dynamiquement   les   tables   de   routage   IP   sur   la   base   des   meilleurs   chemins 
entre routeurs. Il fonctionne en partageant des informations sur l’état des liens entre 
routeurs d’un même système autonome (réseau local d’entreprise, de campus...), et des 
adresses réseaux de ces mêmes liens.
OSPF   est   couramment   utilisé   dans   les   réseaux   d’entreprise.Il   a   de   nombreuses 
fonctionnalités, mais aussi de nombreuses lacunes :
•Il est peu sécurisé ;
•Il   est   complexe,   et   la   plupart   de   ses   fonctionnalités   ne   sont   pas   utilisées,   au  
détriment   de   sa   performance   (délai   de   convergence   du   contenu   des   tables   de 
routage...) et des ressources nécessaires à son fonctionnement (débit) ;
•Il   ne   tient   pas   compte   de   l’état   dynamique   d’un   lien.   Il   se   réfère   aux   données 
statiques préconfigurées, tel que le débit du lien. 
## 2 Objectif du projet
•Concevoir   un   protocole   de   routage   dynamique   simple,   robuste,   sécurisé   et 
nécessitant   peu   de   ressources   de   fonctionnement   (en   mémoire   et   en   échange 
d’information), en remplacement d’OSPF pour un réseau local ;
•Implémenter ce protocole pour des routeurs sous environnement Linux ;
•Valider   le   bon   fonctionnement   du   protocole,   qualifier   ses   performances   (temps 
de convergence) et les ressources nécessaires à son bon fonctionnement.
## 3 Périmètre du projet
•Utiliser un langage de programmation adapté au système (C, C++ ou Rust) ;
•Adapter le protocole à IPv4 uniquement ;
•Valider le bon fonctionnement du protocole avec des routeurs virtualisés ;
•Utiliser le système d’exploitation Linux (pas de contrainte sur la distribution) ;
•Ne pas installer d’interface graphique ;
•Utiliser le réseau de test (en annexe) pour effectuer les tests de fonctionnalités et 
de performances. Les procédures de tests sont à définir ultérieurement.
## 4 Description fonctionnelle 
### 4.1 Priorités de niveau 1
1.1 Calculer   les   meilleurs   chemins   entre   chaque   routeur   et   réseaux   IP   du   réseau  
local sur la base des chemins les plus courts (nombre de saut), de l’état des liens 
(actifs ou inactifs) et de leurs capacités nominales (débits maximaux).
1.2 Mettre à jour le calcul des meilleurs chemins tenant compte des modification 
des caractéristiques du réseau (routeurs, liens réseaux).
1.3 Activer ou désactiver le protocole à la demande.
1.4 Pour chaque routeur, spécifier les interfaces réseaux qui doivent être incluses 
dans le calcul des meilleurs chemins.
1.5 Modifier   la   table   de   routage   IPv4   des   routeurs   en   fonction   du   calcul   des  
meilleurs chemins.
1.6 Mémoriser la liste des routeurs voisins (adresse IP et nom système des routeurs  
voisins) de chaque routeur.
1.7 Afficher à la demande la liste des routeurs voisins d’un routeur.
1.8 Tolérer les pannes du réseau.
### 4.2 Priorités de niveau 2
2.1 Minimiser la quantité d’informations échangées entre les routeurs du réseau 
2.2 Minimiser   la   quantité   de   mémoire   nécessaire   au   bon   fonctionnement   du 
protocole dans les routeurs.
2.3 Minimiser le temps de convergence du protocole.
2.4 Spécifier   un   routeur   comme   routeur   par   défaut   pour   l’ensemble   du   réseau 
(fonctionnalité « default originate » d’OSPF
### 4.3 Priorités de niveau 3
3.1 Calculer les meilleurs chemins entre chaque routeur et réseaux IP du réseau 
local sur la base des chemins les plus courts (nombre de saut), de l’état des liens 
(actifs ou inactifs) et de leurs capacités disponibles (débits disponibles).
3.2 Sécuriser les échanges d’information entre les différentes entités.
