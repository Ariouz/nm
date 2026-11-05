# ft_nm

Reproduction de `nm` sous Linux, en C, suivant le sujet **ft_nm, version 4.0**.
Le programme lit les tables de symboles des ELF **x86 32 bits** et **x86-64** :
fichiers objets `.o`, exécutables classiques ou PIE et bibliothèques partagées
`.so`. Les archives ordinaires GNU/BSD `.a` sont également prises en charge,
y compris les noms longs de leurs membres.

L'organisation reprend celle de `ft_ping` : un header dans `includes/`, un
`main.c`, des structures `t_*`, des fonctions en `snake_case`, puis de petits
fichiers regroupés par rôle dans `srcs/cli/`, `srcs/elf/`, `srcs/utils/` et
`srcs/archive/`. Aucune dépendance à libelf, libbfd ou une libft. Le programme
ne lance aucune commande externe pour analyser les fichiers.

## Compilation et utilisation

Prérequis : Linux, GCC et GNU Make. Compiler `ft_nm` sur un hôte 64 bits suffit
pour analyser aussi les fichiers ELF 32 bits.

```sh
make                    # Partie obligatoire
./ft_nm fichier.o
./ft_nm executable bibliotheque.so
./ft_nm                 # Analyse a.out dans le répertoire courant

make bonus              # Même exécutable ft_nm, avec les cinq options
./ft_nm -a fichier.o
./ft_nm -gu bibliotheque.so
./ft_nm -r fichier.o
./ft_nm -p fichier.o
./ft_nm -- -nom-de-fichier.o
./ft_nm --help

make clean              # Supprime les objets et les exécutables en cache
make fclean             # Supprime également ft_nm
make re                 # Recompile la partie obligatoire
```

Les objets obligatoires et bonus sont séparés dans `.build/mandatory/` et
`.build/bonus/`. Chaque variante possède son exécutable en cache. `make` et
`make bonus` recopient la variante demandée vers `ft_nm` seulement si son
contenu diffère : les appels répétés ne recompilent ni ne relient inutilement.
Les dépendances sur les headers sont générées avec `-MMD -MP`.

| Option bonus | Effet |
| --- | --- |
| `-a` | Inclut les symboles de fichier et de section, notamment les informations de debug. |
| `-g` | Conserve les symboles externes : globaux, faibles et GNU unique. |
| `-u` | Conserve les symboles non définis dans ce fichier. |
| `-r` | Inverse le tri alphabétique des symboles. |
| `-p` | Affiche les symboles dans l'ordre de la table ELF, sans tri. |

Les options se combinent, peuvent apparaître entre les noms de fichiers, et
`--` termine leur interprétation. `-p` prend le dessus sur `-r`, comme avec
GNU `nm`. En mode obligatoire, les cinq options bonus sont rejetées.

## Comprendre un fichier ELF

Un ELF commence par un en-tête qui décrit notamment sa classe, son ordre
d'octets, son architecture et l'emplacement de sa table des sections.
Les offsets désignent des positions **dans le fichier** : ils ne sont pas des
pointeurs utilisables directement par le programme.

Une section décrit une zone comme `.text`, `.data`, `.bss`, `.symtab` ou
`.strtab`. `.symtab` contient les symboles et son champ `sh_link` indique la
table de chaînes qui contient leurs noms. Le nom d'un symbole est un offset
`st_name` dans cette table ; `st_shndx` indique sa section ; `st_info` contient
son type et sa liaison ; `st_value` contient sa valeur.

`.bss` est de type `SHT_NOBITS` : elle décrit de la mémoire réservée lors du
chargement, sans stocker tous ces octets dans le fichier. Il serait incorrect
de lui appliquer les mêmes contrôles de contenu qu'à `.text`.

Un exécutable supprimé de ses symboles avec `strip` peut encore contenir une
`.dynsym` destinée au chargeur dynamique. Sans option `-D`, GNU `nm` consulte
la table ordinaire `.symtab` : `ft_nm` fait de même et signale `no symbols`
quand cette table ne fournit aucun symbole. Cela ne constitue pas une erreur
de lecture du fichier.

## Exécution étape par étape

### 1. Initialiser et analyser les arguments

`main()` initialise la collation depuis la locale, puis appelle
`init_cli_options()` et `parse_cli_options()`.

`t_opts` contient les cinq booléens des bonus et un tableau de noms de fichiers.
Le tableau contient les pointeurs reçus dans `argv` : les chaînes ne sont pas
dupliquées. Une option inconnue produit une erreur et le tableau est libéré.
Sans fichier, la cible devient `a.out`.

### 2. Lire un fichier

`process_file()` appelle `open_file()`. `stat()` vérifie que le chemin désigne
un fichier ordinaire : un dossier ou un FIFO est refusé avant l'ouverture.
`fopen()`, `malloc()` et `fread()` chargent ensuite les octets dans un buffer.
`fclose()` ferme le flux, y compris sur les chemins d'échec.

L'analyse travaille sur cette copie du fichier. Elle ne dépend donc plus d'un
mapping susceptible de provoquer un `SIGBUS` si le fichier original est
tronqué après sa lecture. Une lecture incomplète est signalée.

Si les premiers octets sont `!<arch>\n`, le fichier est une archive ordinaire :
`process_archive()` parcourt ses membres, contrôle leurs tailles et leurs noms,
puis transmet chaque objet à `process_object()`. Les tables d'index d'archive
sont ignorées. Les membres empruntent une portion du buffer principal, sans
la recopier ni la libérer individuellement.

### 3. Reconnaître et vérifier l'ELF

`init_elf()` remet `t_elf` à zéro. La signature `\x7fELF`, la classe 32/64 bits,
l'ordre des octets, les versions et la machine sont contrôlés. Les formats
acceptés sont `ET_REL`, `ET_EXEC` et `ET_DYN`, avec les machines `EM_386` ou
`EM_X86_64` correspondant à la classe.

`read_number()` reconstruit les entiers octet par octet. Le code évite de
convertir directement le buffer en `Elf64_Ehdr *` ou `Elf64_Sym *` : cela
évite les lectures non alignées et permet de respecter l'ordre des octets
indiqué dans l'en-tête.

### 4. Lire les sections

`read_sections()` vérifie la taille des entrées et les limites de la table
avant de l'allouer. `read_section()` transforme chaque entrée en `t_section`,
une représentation commune aux deux classes ELF.

La numérotation étendue est gérée : un `e_shnum` nul avec une table présente
stocke le nombre réel de sections dans la section zéro ; `SHN_XINDEX` pour
`e_shstrndx` stocke l'index réel de la table de noms dans cette même section.
`PN_XNUM` est également lu pour le nombre d'en-têtes de programme.

Chaque zone de contenu doit rester dans le buffer, sauf `SHT_NOBITS`, et les
alignements non nuls doivent être des puissances de deux. Les noms de sections
sont contrôlés avec `get_string()` avant toute comparaison ou impression.

### 5. Extraire les symboles

`read_symbols()` recherche les sections `SHT_SYMTAB`. Pour chaque table :

1. Vérifier sa taille d'entrée : 16 octets en ELF32, 24 octets en ELF64.
2. Vérifier que la taille totale est un multiple de cette taille.
3. Vérifier `sh_link` et le type `SHT_STRTAB` de la table de noms.
4. Ignorer l'entrée zéro, réservée au symbole nul ELF.
5. Lire les champs, puis contrôler le nom et l'index de section.
6. Résoudre `SHN_XINDEX` à l'aide d'une table `SHT_SYMTAB_SHNDX`, si nécessaire.
7. Déterminer la lettre correspondant au symbole.

Les noms restent des pointeurs dans le buffer du fichier. Un symbole de
section sans nom utilise le nom de sa section. Pour un symbole `SHN_COMMON`,
la valeur affichée est sa **taille**, plutôt que son alignement `st_value`,
comme avec GNU `nm`. Pour un objet relogeable, l'adresse de la section est
ajoutée à la valeur du symbole.

### 6. Classer les symboles

`get_symbol_type()` examine d'abord les liaisons particulières, puis la
section et ses flags. Quelques exemples :

| Lettre | Signification |
| --- | --- |
| `T` / `t` | Code, généralement dans `.text`. |
| `D` / `d` | Données initialisées et modifiables. |
| `B` / `b` | Données de type `SHT_NOBITS`, généralement `.bss`. |
| `R` / `r` | Données allouées en lecture seule. |
| `A` / `a` | Symbole absolu, avec `SHN_ABS`. |
| `C` | Symbole commun, avec `SHN_COMMON`. |
| `U` | Symbole non défini, avec `SHN_UNDEF`. |
| `W` / `w` | Symbole faible défini / non défini. |
| `V` / `v` | Objet faible défini / non défini. |
| `u` | Symbole GNU unique. |
| `i` | Fonction indirecte GNU, `STT_GNU_IFUNC`. |
| `N` | Symbole de debug ou de section non allouée correspondant à cette classe. |
| `n` | Symbole local d'une section non allouée en lecture seule, notamment d'un groupe COMDAT C++. |
| `?` | Type sans classification connue. |

Pour les types ordinaires, la minuscule désigne un symbole local et la
majuscule un symbole externe. Les types GNU unique, IFUNC, faibles et debug
possèdent leurs propres conventions.

### 7. Trier et filtrer

`sort_symbols()` utilise un tri fusion stable en `O(n log n)` avec un buffer
temporaire. `strcoll()` compare les noms en tenant compte de la locale.
En cas d'égalité, les noms bruts puis les valeurs départagent les symboles.
`-r` inverse la comparaison et `-p` désactive tout le tri.

`keep_symbol()` applique ensuite les filtres. Par défaut, les entrées
`STT_FILE` et `STT_SECTION` sont masquées ; `-a` les conserve. `-g` élimine
les liaisons locales et `-u` élimine les symboles définis.

### 8. Afficher et libérer

`print_symbols()` écrit une valeur hexadécimale sur 8 caractères en ELF32 ou
16 en ELF64, suivie de la lettre et du nom. Les symboles non définis ont un
champ de valeur rempli d'espaces. Cette valeur est une information du fichier
ELF ; elle ne représente pas nécessairement l'adresse effective d'un processus
en cours d'exécution, notamment avec les PIE et l'ASLR.

`process_object()` libère le tableau des symboles et celui des sections,
même après une erreur. `process_file()` libère ensuite le buffer du fichier.
Enfin, `main()` libère le tableau d'arguments.

Une erreur sur un fichier n'empêche pas de traiter les fichiers suivants.
Les erreurs vont sur `stderr`, les symboles sur `stdout`. Le statut de sortie
compte les fichiers dont le traitement a échoué, comme GNU `nm` ; le système
ne conserve que les huit bits de poids faible du statut.

## Fonctions à connaître

| Fonction | Pourquoi elle est importante |
| --- | --- |
| `valid_range()` | Vérifie `offset <= taille` puis `longueur <= taille - offset`, sans addition susceptible de déborder. |
| `read_number()` | Lit un entier ELF sans cast de structure ni hypothèse d'alignement. Son appelant doit avoir vérifié la zone contenant les champs. |
| `get_string()` | Vérifie l'offset et cherche un `\0` uniquement dans les limites de la table avec `memchr()`. |
| `read_section()` | Décode les champs d'une section selon la classe ELF. |
| `resolve_section()` | Résout les index de section étendus des symboles et vérifie la table associée. |
| `get_symbol_type()` | Produit la lettre affichée à partir de la liaison, du type et de la section. |
| `merge_sort()` | Garantit une complexité prévisible et une profondeur de pile logarithmique. |
| `process_object()` | Centralise la lecture, le tri, l'affichage et la libération d'un objet. |
| `file_error()` | Écrit un diagnostic et fournit un statut d'erreur, sans interrompre le traitement des autres fichiers. |

Les fonctions de bibliothèque importantes sont `stat()` (métadonnées Linux,
via libc), `fopen()` / `fread()` / `fclose()` (lecture), `malloc()` / `realloc()` /
`free()` (mémoire), `memchr()` (recherche bornée), `strcoll()` (tri selon la
locale) et `printf()` / `fprintf()` (sorties). Les définitions ELF viennent
du header système `<elf.h>`.

## Vérification

```sh
make test
make test-memory
```

`make test` compile les bonus et génère les fixtures dans un répertoire
temporaire, supprimé à la fin. Il nécessite Python 3.9+, GCC et les outils GNU
`nm`, `ar` et `objcopy`. La génération d'objets 32 bits utilise `gcc -m32` ;
les tests 32 bits ne nécessitent pas l'installation d'une libc 32 bits.

La suite réalise **1443 vérifications** : comparaison exacte de la sortie et
du statut avec GNU `nm` sur ELF32/64, exécutables PIE et non-PIE, bibliothèques
partagées, archives à noms longs, toutes les combinaisons des cinq options,
symboles faibles/communs/uniques/IFUNC/debug/COMDAT, combinaisons des flags
de sections, sections à adresse non
nulle, numérotation étendue, fichiers sans symboles, arguments et erreurs.
Elle teste aussi les troncatures, les offsets invalides et les chaînes sans
terminateur en ELF32 et ELF64, 800 mutations déterministes d'ELF et 200 buffers
aléatoires. Un timeout détecte les blocages.

`make test-memory` construit deux exécutables temporaires :

- Une version AddressSanitizer/UndefinedBehaviorSanitizer qui exécute la même
  suite pour vérifier les accès mémoire et les comportements indéfinis.
- Une version liée avec `--wrap=malloc`, `--wrap=realloc` et `--wrap=free`, qui
  vérifie toutes les allocations explicites du projet et rejoue la suite.
  Elle teste aussi **144 scénarios** d'échec d'allocation : lecture simple,
  plusieurs fichiers, archive, aide, option invalide et fichier absent.

Le suivi refuse les doubles libérations et toute allocation du projet encore
ouverte à la fin. Il n'intercepte pas les allocations internes de libc.
LeakSanitizer est désactivé dans cette commande car il ne fonctionne pas
lorsque le processus est exécuté sous `ptrace` dans l'environnement de travail.
Le contrôle des libérations est assuré séparément par le suivi d'allocations.

Pour vérifier avec Valgrind dans un environnement où il est installé :

```sh
make bonus
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./ft_nm -a ./ft_nm
```

Pour une comparaison manuelle reproductible, fixer la même locale :

```sh
LC_ALL=C nm -a fichier.o > reference.txt
LC_ALL=C ./ft_nm -a fichier.o > resultat.txt
diff -u reference.txt resultat.txt
```

Les archives thin (`!<thin>`) et les architectures autres que x86 sont
refusées. Les options GNU supplémentaires, notamment `-D` et `-C`, ne font
pas partie des cinq bonus demandés et ne sont pas implémentées.

## Ressources et assistance

La grille locale `nm.html` a été lue pour vérifier les critères de compilation,
d'erreurs, d'exécutables, d'arguments multiples, d'objets, de bibliothèques
partagées, de mémoire et de bonus. Ses pièces jointes `easy_test.c`,
`not_so_easy_test.c` et les fichiers invalides restent hébergés sur le CDN de
42, inaccessible depuis cet environnement : leurs fichiers exacts n'ont pas
été testés. La suite produit ses propres fixtures pour les mêmes catégories.

La grille mentionne aussi `.dylib`, `lipo` et les binaires universels macOS.
Ces formats Mach-O ne font pas partie du sujet fourni, qui demande explicitement
Linux et les ELF x86 32/64 bits. L'implémentation suit ce périmètre ELF ; elle
ne prétend pas valider le critère macOS de cette grille.

Références locales utiles : `man nm`, `man elf`, `man 3 fread`, `man 3 malloc`
et les structures `Elf32_*` / `Elf64_*` dans `/usr/include/elf.h`.
`readelf -h`, `readelf -SW` et `readelf -sW` permettent de suivre les étapes
du README sur un fichier réel.

L'implémentation, la documentation et les tests ont été rédigés avec
l'assistance de Codex, puis vérifiés par compilation, comparaison avec GNU
`nm`, sanitizers et suivi des allocations. Le déroulement ci-dessus permet
de reprendre chaque choix technique et de le vérifier lors de la soutenance.
