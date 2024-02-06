
vcpkg install nlohmann::json
vcpkg install curl[core,openssl]

TODO : 
- Supprimer les dossiers qui ont été supprimé des deux coté aussi (ajouter la sync des dossiers)
- Ajouter un lock guard sur le .fileshare
- Cleanup des auth-token au moment de logout + verif du timestamp des auth-token
- Ajouter des tests unitaires sur l'ensemble des fonctionnalités