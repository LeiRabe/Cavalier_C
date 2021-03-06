#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <inttypes.h>

#include <signal.h>
#include <sys/signalfd.h>
#include <gtk/gtk.h>

#define MAXDATASIZE 256

/* Variables globales */
int damier[8][8]; // tableau associe au damier
int couleur;      // 0 : pour noir, 1 : pour blanc, 3 : pour pion, 4 : pion avec cavalier noir, 5 : pion avec cavalier blanc

int port; // numero port passé lors de l'appel

int msg; //informations envoyées via la socket

char *addr_j2, *port_j2; // Info sur adversaire

pthread_t thr_id; // Id du thread fils gerant connexion socket

int sockfd, newsockfd = -1;                      // descripteurs de socket
int addr_size;                                   // taille adresse
struct sockaddr *their_addr;                     // structure pour stocker adresse adversaire
struct addrinfo s_init, *servinfo, *p, s_client; // voir tp

fd_set master, read_fds, write_fds; // ensembles de socket pour toutes les sockets actives avec select
int fdmax;                          // utilise pour select

/* Variables globales associées à l'interface graphique */
GtkBuilder *p_builder = NULL;
GError *p_err = NULL;

// Entetes des fonctions

/* Fonction permettant afficher image pion dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_pion(int col, int lig);

/* Fonction permettant afficher image cavalier noir dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_noir(int col, int lig);

/* Fonction permettant afficher image cavalier blanc dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_blanc(int col, int lig);

/* Fonction transformant coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar *coord, int *col, int *lig);

/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case);

/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void);

/* Fonction retournant texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void);

/* Fonction retournant texte du champs login de l'interface graphique */
char *lecture_login(void);

/* Fonction retournant texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void);

/* Fonction retournant texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void);

/* Fonction affichant boite de dialogue si partie gagnee */
void affiche_fenetre_gagne(void);

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void);

/* Fonction appelee lors du clique du bouton Se connecter */
static void clique_connect_serveur(GtkWidget *b);

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void);

/* Fonction appelee lors du clique du bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget *b);

/* Fonction desactivant les cases du damier */
void gele_damier(void);

/* Fonction activant les cases du damier */
void degele_damier(void);

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void);

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void);

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port);

/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar *coord, int *col, int *lig)
{
  char *c;

  c = malloc(3 * sizeof(char));

  c = strncpy(c, coord, 1);
  c[1] = '\0';

  if (strcmp(c, "A") == 0)
  {
    *col = 0;
  }
  if (strcmp(c, "B") == 0)
  {
    *col = 1;
  }
  if (strcmp(c, "C") == 0)
  {
    *col = 2;
  }
  if (strcmp(c, "D") == 0)
  {
    *col = 3;
  }
  if (strcmp(c, "E") == 0)
  {
    *col = 4;
  }
  if (strcmp(c, "F") == 0)
  {
    *col = 5;
  }
  if (strcmp(c, "G") == 0)
  {
    *col = 6;
  }
  if (strcmp(c, "H") == 0)
  {
    *col = 7;
  }

  *lig = atoi(coord + 1) - 1;
}

/* Fonction transforme coordonnees du damier graphique en indexes pour matrice du damier */
void indexes_to_coord(int col, int lig, char *coord)
{
  char c;

  if (col == 0)
  {
    c = 'A';
  }
  if (col == 1)
  {
    c = 'B';
  }
  if (col == 2)
  {
    c = 'C';
  }
  if (col == 3)
  {
    c = 'D';
  }
  if (col == 4)
  {
    c = 'E';
  }
  if (col == 5)
  {
    c = 'F';
  }
  if (col == 6)
  {
    c = 'G';
  }
  if (col == 7)
  {
    c = 'H';
  }

  sprintf(coord, "%c%d", c, lig + 1);
}

/* Fonction permettant afficher image pion dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_pion(int col, int lig)
{
  char *coord;

  coord = malloc(3 * sizeof(char));
  indexes_to_coord(col, lig, coord);

  gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_pion.png");
}

/* Fonction permettant afficher image cavalier noir dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_noir(int col, int lig)
{
  char *coord;

  coord = malloc(3 * sizeof(char));
  indexes_to_coord(col, lig, coord);

  gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_cav_noir.png");
}

/* Fonction permettant afficher image cavalier blanc dans case du damier (indiqué par sa colonne et sa ligne) */
void affiche_cav_blanc(int col, int lig)
{
  char *coord;

  coord = malloc(3 * sizeof(char));
  indexes_to_coord(col, lig, coord);

  gtk_image_set_from_file(GTK_IMAGE(gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_cav_blanc.png");
}

/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case)
{
  int col, lig, type_msg, nb_piece, score;
  char buf[MAXDATASIZE];

  // Traduction coordonnees damier en indexes matrice damier
  coord_to_indexes(gtk_buildable_get_name(GTK_BUILDABLE(gtk_bin_get_child(GTK_BIN(p_case)))), &col, &lig);

  //récupération de la position initiale du cavalier
  int colCavalier; //colonne
  int ligCavalier; //ligne

  //A FAIRE : réception de l'état de jeu de l'adversaire
  //mise à jour du damier local
  // ~/!\~ la première fois on ne reçoit rien
  //si score = -1 ==> bloqué : est-ce qu'on peut jouer ?
  //                           non ? égalité send état
  //                           oui ? victoire send etat
  //si score = 0 ==> rien : on peut jouer son tour
  //A TRAVAILLER (bonus) : défaite du cavalier blanc car le cavalier noir est à l’abri derrière une barrière de pions rouge
  
  //parcours du damier local pour trouver le cavalier qui joue en fonction de sa couleur
  for(int i=0; i<8; i++){
    for(int j=0; j<8; j++){
      if(damier[i][j]==couleur){
        colCavalier = i;
        ligCavalier = j;
        break;
      }
    }
  } 

  //tableau de possibilités
  int valeurs [8] = {-2,-2,-2,-2,-2,-2,-2,-2}; //initialisé pour savoir : -2 hors damier, -1 libre, 0 cavalier noir
  int nb_chemin_possible = 0; //pour déterminer l'état du jeu, il faut savoir les mouvements possibles
  score=0; // détermine l'état du jeu : -1 bloqué (plus de mouvements possibles), 0 neutre (partie en cours), 1 gagné

  //vérification des coups possibles en excluant les cases qui ne sont pas dans le damier
  if( (0<=colCavalier+1) && (colCavalier+1<8) && (0<=ligCavalier-2) && (ligCavalier-2<8) ){
      valeurs[0] = damier[colCavalier+1][ligCavalier-2];
  }
  if( (0<=colCavalier-1) && (colCavalier-1<8) && (0<=ligCavalier-2) && (ligCavalier-2<8) ){
    valeurs[1] = damier[colCavalier-1][ligCavalier-2];
  }
  if( (0<=colCavalier+1) && (colCavalier+1<8) && (0<=ligCavalier+2) && (ligCavalier+2<8) ){
    valeurs[2] = damier[colCavalier+1][ligCavalier+2];
  }
  if( (0<=colCavalier-1) && (colCavalier-1<8) && (0<=ligCavalier+2) && (ligCavalier+2<8) ){
    valeurs[3] = damier[colCavalier-1][ligCavalier+2];
  }
  if( (0<=colCavalier+2) && (colCavalier+2<8) && (0<=ligCavalier-1) && (ligCavalier-1<8) ){
    valeurs[4] = damier[colCavalier+2][ligCavalier-1];
  }
  if( (0<=colCavalier-2) && (colCavalier-2<8) && (0<=ligCavalier-1) && (ligCavalier-1<8) ){
    valeurs[5] = damier[colCavalier-2][ligCavalier-1];
  }
  if( (0<=colCavalier+2) && (colCavalier+2<8) && (0<=ligCavalier+1) && (ligCavalier+1<8) ){
    valeurs[6] = damier[colCavalier+2][ligCavalier+1];
  }
  if( (0<=colCavalier-2) && (colCavalier-2<8) && (0<=ligCavalier+1) && (ligCavalier+1<8) ){
    valeurs[7] = damier[colCavalier-2][ligCavalier+1];
  }

  //0 = noir, 1= blanc, 3 = pion

  //vérification de l'état du jeu pour le cavalier qui joue
  for (int i = 0; i < 8; i++){
    //comptage du nombre de pions pour savoir si le cavalier est bloqué
    if ( (valeurs[i] == -1))
      nb_chemin_possible += 1;
    
    //vérification que le cavalier blanc peut atteindre le cavalier noir en un coup
    else if ( (couleur == 1) && (valeurs[i] == 0) )
      score = 1; //si oui, il a gagné la partie
  }

  //s'il n'y a pas de chemin possible, on est bloqué
  if (nb_chemin_possible <= 0)
    score = -1; //cavalier bloqué
    
  if( (score == 0) && (damier[col][lig]==-1)){ //partie en cours et case vide sélectionnée

    //vérification d'un coup valide
    if( ((abs(colCavalier-col)==1) && (abs(ligCavalier-lig)==2)) || ((abs(colCavalier-col)==2) && (abs(ligCavalier-lig)==1)) ) {
      
      //placement d'un pion rouge à l'ancienne position du cavalier
      affiche_pion(colCavalier,ligCavalier); 

      //déplacement du cavalier à la nouvelle position
      if(couleur==0){ //cavalier noir
        affiche_cav_noir(col,lig); 
      }
      else { //cavalier blanc
        affiche_cav_blanc(col,lig);
      }

      //mise à jour des nouvelles coordonnées du cavalier dans le damier local
      damier[col][lig] = couleur;

      //mise à jour en ajoutant un nouveau pion dans le damier local
      damier[colCavalier][ligCavalier] = 3;
    }
  }
  //A FAIRE
  //envoi des données
  //allouer l'espace du buffer dans la mémoire
  memset(&buf,0, sizeof(buf));
  //définir les nouvelles coord du cavalier dans le buffer 
  snprintf(buf,MAXDATASIZE,"%d,%d",htons((uint16_t)(int)&col),htons((uint16_t)(int)&lig));
  size_t lenBuf = strlen(buf);
 
  if(send(newsockfd, buf, lenBuf , 0) == -1) {
      perror("send");
  }
  //après l'appel de coup joueur on gèle le damier
  gele_damier();
  //Avant de passer la main au cavalier blanc: on va devoir fournir toutes les info du noir au blanc pour qu'il puisse comprendre les coups du joueur noir
  //comment mettre en place le tour par tour ? -> Comment passer la main au joueur adverse ? ->
  //1. envoyer un msg en mode: "c'est à ton tour"
  //2. côté graphique: 2 fonctions gele_damier et degele_damier, ça rend clicable ou non les cases du damier

  //envoie des info va se faire ici
  //de l'autre côté on reçoit les données mais où allons nous placer ça ?
}

/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void)
{
  GtkWidget *entry_addr_srv;

  entry_addr_srv = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_adr");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_srv));
}

/* Fonction retournant texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void)
{
  GtkWidget *entry_port_srv;

  entry_port_srv = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_port");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_srv));
}

/* Fonction retournant texte du champs login de l'interface graphique */
char *lecture_login(void)
{
  GtkWidget *entry_login;

  entry_login = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_login");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_login));
}

/* Fonction retournant texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void)
{
  GtkWidget *entry_addr_j2;

  entry_addr_j2 = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_addr_j2");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_addr_j2));
}

/* Fonction retournant texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void)
{
  GtkWidget *entry_port_j2;

  entry_port_j2 = (GtkWidget *)gtk_builder_get_object(p_builder, "entry_port_j2");

  return (char *)gtk_entry_get_text(GTK_ENTRY(entry_port_j2));
}

/* Fonction affichant boite de dialogue si partie gagnee */
void affiche_fenetre_gagne(void)
{
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez gagné!!!");
  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void)
{
  GtkWidget *dialog;

  GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

  dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez perdu!");
  gtk_dialog_run(GTK_DIALOG(dialog));

  gtk_widget_destroy(dialog);
}

/* Fonction appelee lors du clique du bouton Se connecter */
static void clique_connect_serveur(GtkWidget *b)
{
  /***** TO DO *****/
}

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void)
{
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "button_start"), FALSE);
}

/* Fonction traitement signal bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget *b)
{
  if (newsockfd == -1)
  {
    // Deactivation bouton demarrer partie
    gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "button_start"), FALSE);

    // Recuperation  adresse et port adversaire au format chaines caracteres
    addr_j2 = lecture_addr_adversaire();
    port_j2 = lecture_port_adversaire();

    printf("[Port joueur : %d] Adresse j2 lue : %s\n", port, addr_j2);
    printf("[Port joueur : %d] Port j2 lu : %s\n", port, port_j2);

    pthread_kill(thr_id, SIGUSR1);
  }
}

/* Fonction desactivant les cases du damier */
void gele_damier(void)
{
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH1"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH2"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH3"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH4"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH5"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH6"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH7"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG8"), FALSE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH8"), FALSE);
}

/* Fonction activant les cases du damier */
void degele_damier(void)
{
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH1"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH2"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH3"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH4"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH5"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH6"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH7"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxA8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxB8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxC8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxD8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxE8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxF8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxG8"), TRUE);
  gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "eventboxH8"), TRUE);
}

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void)
{
  // Initialisation du damier (A1=cavalier_noir, H8=cavalier_blanc)
  affiche_cav_blanc(7, 7);
  affiche_cav_noir(0, 0);

  /***** TO DO *****/
  //ajouter les cavaliers au damier 2D
  //0 : pour noir, 1 : pour blanc
  damier[0][0] = 0;
  damier[7][7] = 1;
}

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void)
{
  GtkTextIter start, end;

  gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start);
  gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &end);

  gtk_text_buffer_delete(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start, &end);
}

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port)
{
  const gchar *joueur;

  joueur = g_strconcat(login, " - ", adresse, " : ", port, "\n", NULL);

  gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), joueur, strlen(joueur));
}

/* Fonction exécutée par le thread gérant les communications à travers la socket */
/*Gestion des réceptions des messages*/
static void *f_com_socket(void *p_arg)
{
  //boucle infini qui va monitorer les sockets
  //création ou destruction de socket, mettre à jour les variables qui s'appellent master
  //à chq creation de socket: fd_set(sock,*master)
  //faire un test (if sock > fd_max): fd_max = sock
  //à la destruction d'un socket: fd_clr(sock,*master)

  //Tout le traitement de la réception des elt venant du joueur adverse doit se passer ici
  int i, nbytes, col, lig;

  char buf[MAXDATASIZE], *tmp, *p_parse;
  int len, bytes_sent, t_msg_recu;

  sigset_t signal_mask;
  int fd_signal;

  uint16_t type_msg, col_j2;
  uint16_t ucol, ulig;

  /* Association descripteur au signal SIGUSR1 */
  sigemptyset(&signal_mask);
  sigaddset(&signal_mask, SIGUSR1);

  if (sigprocmask(SIG_BLOCK, &signal_mask, NULL) == -1)
  {
    printf("[Pourt joueur %d] Erreur sigprocmask\n", port);

    return 0;
  }

  fd_signal = signalfd(-1, &signal_mask, 0);

  if (fd_signal == -1)
  {
    printf("[port joueur %d] Erreur signalfd\n", port);

    return 0;
  }

  /* Ajout descripteur du signal dans ensemble de descripteur utilisé avec fonction select */
  FD_SET(fd_signal, &master);

  if (fd_signal > fdmax)
  {
    fdmax = fd_signal;
  }

  while (1)
  {
    read_fds = master; // copie des ensembles

    if (select(fdmax + 1, &read_fds, &write_fds, NULL, NULL) == -1)
    {
      perror("Problème avec select");
      exit(4);
    }

    printf("[Port joueur %d] Entree dans boucle for\n", port);
    for (i = 0; i <= fdmax; i++)
    {
      printf("[Port joueur %d] newsockfd=%d, iteration %d boucle for\n", port, newsockfd, i);

      if (FD_ISSET(i, &read_fds))
      {
        if (i == fd_signal)
        {
          /* Cas où de l'envoie du signal par l'interface graphique pour connexion au joueur adverse */

          int check_addr; //vérification de addrinfo
          //partie client
          /***** TO DO *****/
          //avant de faire le connect
          //fermer la socket du main
          //créer une nouvelle socket cliente
          //connect

          //ferme la socket d'écoute dans le main
         // close(sockfd);

          //supp
          FD_CLR(sockfd, &master);

          //clear le fd_signal
          FD_CLR(fd_signal, &master);

          memset(&s_client, 0, sizeof(s_client));
          s_client.ai_family = AF_UNSPEC;
          s_client.ai_socktype = SOCK_STREAM;
          check_addr = getaddrinfo(addr_j2, port_j2, &s_init, &servinfo);

          if (check_addr != 0)
          {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(check_addr));
            exit(1);
          }

          for (p = servinfo; p != NULL; p = p->ai_next)
          {
            //creer la socket de service du client
            if ((newsockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            {
              perror("client: socket");
              continue;
            }

            if ((connect(newsockfd, p->ai_addr, p->ai_addrlen)) == -1)
            {
              close(newsockfd);
              perror("client: connect");
              continue;
            }
            break;
          }

          //ajouter la nouvelle socket dans master
          FD_SET(newsockfd, &master);

          //tester si newsockfd > fdmax si oui alors fdmax = newsockfd
          if (fd_signal > fdmax)
          {
            fdmax = fd_signal;
          }

          //choix couleur pour le cavalier: client -> noir
          couleur = 0;

          //initialisation du plateau
          init_interface_jeu();
          printf("start jeu");
          //dégèle du plateau pour pouvoir jouer
          degele_damier();
         
        }

        if (i == sockfd)
        { // Acceptation connexion adversaire

          //partie serveur - socket d'écoute
          //création de la socket d'écoute dans le main

          /***** TO DO *****/
          socklen_t s_taille = sizeof(their_addr);
     
       
          //accept est bloquante, accept retourne une nouvelle socket et c'est cette socket
          //que l'on va utiliser pour communiquer avec le client
          newsockfd = accept(sockfd, (struct sockaddr *)&their_addr, &s_taille);
          
          if (newsockfd == -1)
          {
            perror("erreur lors du accept");
            continue;
          }

          if (newsockfd > fdmax)
          {
            fdmax = newsockfd;
          }

          FD_SET(newsockfd, &master);
          close(sockfd);
          FD_CLR(sockfd,&master);

          //indiquer que le connexion est établie
          printf("Serveur: connexion d'un nouveau client\n");

          //choix couleur pour le cavalier: server -> blanc
          couleur = 1;

          //initialisation du plateau
          init_interface_jeu();

          //initialiser les damiers (positions des pions)
          gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, "button_start"), FALSE);

          //gèle du plateau pour donner la main à l'autre joueur
          gele_damier();
        }

        else
        { // Reception et traitement des messages du joueur adverse
         memset(buf,0,sizeof(buf));
          //memset
          recv(newsockfd, buf, sizeof(buf), 0);
          degele_damier();
          /***** TO DO *****/
          //quelles sont les info qu'on va échanger?
          //envoie des données se fait dans coup joueur
          printf("On ne fait rien encore");
          //gérer la couleur qui est sera blanc qui sera noir (pour le client serveur)
        }
      }
    }
  }

  return NULL;
}

int main(int argc, char **argv)
{
  int i, j, ret;

  if (argc != 2)
  {
    printf("\nPrototype : ./othello num_port\n\n");

    exit(1);
  }

  /* Initialisation de GTK+ */
  gtk_init(&argc, &argv);

  /* Creation d'un nouveau GtkBuilder */
  p_builder = gtk_builder_new();

  if (p_builder != NULL)
  {
    /* Chargement du XML dans p_builder */
    gtk_builder_add_from_file(p_builder, "UI_Glade/Cavalier.glade", &p_err);

    if (p_err == NULL)
    {
      /* Recupération d'un pointeur sur la fenetre. */
      GtkWidget *p_win = (GtkWidget *)gtk_builder_get_object(p_builder, "window1");

      /* Gestion evenement clic pour chacune des cases du damier */
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH1"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH2"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH3"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH4"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH5"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH6"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH7"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxA8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxB8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxC8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxD8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxE8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxF8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxG8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "eventboxH8"), "button_press_event", G_CALLBACK(coup_joueur), NULL);

      /* Gestion clic boutons interface */
      g_signal_connect(gtk_builder_get_object(p_builder, "button_connect"), "clicked", G_CALLBACK(clique_connect_serveur), NULL);
      g_signal_connect(gtk_builder_get_object(p_builder, "button_start"), "clicked", G_CALLBACK(clique_connect_adversaire), NULL);

      /* Gestion clic bouton fermeture fenetre */
      g_signal_connect_swapped(G_OBJECT(p_win), "destroy", G_CALLBACK(gtk_main_quit), NULL);

      /* Recuperation numero port donne en parametre */
      port = atoi(argv[1]);

      /* Initialisation du damier de jeu */
      for (i = 0; i < 8; i++)
      {
        for (j = 0; j < 8; j++)
        {
          damier[i][j] = -1;
        }
      }

      /***** TO DO *****/

      // Initialisation socket et autres objets, et création thread pour communications avec joueur adverse
      //toutes les init doivent se faire ici
      //chq programmes seront composés de 2 proccessus -> un thread fils qui va executé f_com_socket et le père va gérer l"interface graphique
      //processus en tache de fond doit être nécessairement un thread

      //initialiser la socket d'écoute

      memset(&s_init, 0, sizeof(s_init));
      s_init.ai_family = AF_INET;
      s_init.ai_socktype = SOCK_STREAM;
      //attendre que l'on se connecte à nous et maintenir la connexion ouverte pour
      //l'écoute du client, toujours on ne ferme pas, maintenir la session ouverte
      s_init.ai_flags = AI_PASSIVE;

      if (getaddrinfo(NULL, argv[1], &s_init, &servinfo) != 0)
      {
        //fprintf(stdout, "%s\n", );
        fprintf(stderr, "Erreur getaddrinfo\n");
        exit(1);
      }

      p = servinfo;
      //créer la socket d'écoute
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
      {
        perror("Serveur: socket");
      }

      //bind car c'est un programme server
      if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
      {
        close(sockfd);
        perror("Serveur: erreur bind");
      }

      freeaddrinfo(servinfo);

      if (listen(sockfd, 1) == -1)
      {
        perror("erreur lors du listen");
        exit(1);
      }

      //création thread qui va appeler le f_com_socket et déter si on est un client ou un server
      if (pthread_create(&thr_id, NULL, f_com_socket, NULL) == -1)
      {
        fprintf(stderr, "erreur de creation pthread numero %d\n", &thr_id);
      }
      printf("je suis la thread initiale %d.%d\n", getpid(), pthread_self());

      //init la master à 0
      FD_ZERO(&master);

      FD_SET(sockfd, &master);

      fdmax = sockfd;

      gtk_widget_show_all(p_win);
      gtk_main();
    }
    else
    {
      /* Affichage du message d'erreur de GTK+ */
      g_error("%s", p_err->message);
      g_error_free(p_err);
    }
  }

  return EXIT_SUCCESS;
}
