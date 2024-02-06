
vcpkg install nlohmann::json
vcpkg install curl[core,openssl]

TODO : 
- Supprimer les dossiers qui ont été supprimé des deux coté aussi (en gros ajouter la sync des dossiers)
- Ajouter des tests unitaires
- Ajouter un lock guard sur le .fileshare