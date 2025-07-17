#ifndef braindisplay_h
#define braindisplay_h

#define BIGCHAR 4
#define SMALLCHAR 2

#define SCREEN_WIDTH 280
#define SCREEN_HEIGHT 240

#include <TFT_eSPI.h>
#include "FS.h"
#include "FFat.h"
#include "NotoSansBold15.h"
#include "NotoSansBold36.h"
#include "NotoSansMonoSCB20.h"
#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_LARGE NotoSansMonoSCB20
#define AA_FONT_HUGE NotoSansBold36
//#include <oschammond.h>

#ifdef HAMMOND
#include <oschammond.h>
#else
#include <oscfloat.h>
#endif

int peak_filt[] ={0,1,1,1,1,2,2,2,2,3,3,3,4,4,5,5,6,7,8,10,12,14,17,20,24,27,29,31,32,32,33,33,34,34,34,35,35,35,35,34,34,34,33,33,33,32,32,31,31,30,30,29,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};

void but_record();

class Bdisplay
{
  
public:
	
	
	
	uint8_t menuminmax[16][2];
	uint8_t paramdrawnum[128];
	String list_text[20];
	String list_previous;
	uint8_t list_size=0;
	uint8_t line_selected[4];
	uint8_t line_selected_max[4];
	uint8_t menu_level;
	
	TFT_eSPI tft = TFT_eSPI();
	TFT_eSprite spr = TFT_eSprite(&tft);
	TFT_eSprite sprt = TFT_eSprite(&tft);
	
	bool valida;
	
	// Buffers pour les fichiers préchargés
	char* menutitleBuffer = NULL;
	size_t menutitleSize = 0;
	char* menulistBuffer  = NULL;
	size_t menulistSize = 0;
	char* menuorgBuffer  = NULL;
	size_t menuorgSize = 0;
	
	Bdisplay() {
		spr.setColorDepth(8);      // Create an 8bpp Sprite of 60x30 pixels
        spr.createSprite(280, 160);  // 8bpp requires 64 * 30 = 1920 bytes
        //spr.setPivot(32, 55);      // Set pivot relative to top left corner of Sprite
        spr.fillSprite(TFT_BLACK); // Fill the Sprite with black
		
		sprt.setColorDepth(8);      // Create an 8bpp Sprite of 60x30 pixels
        sprt.createSprite(280, 40);  // 8bpp requires 64 * 30 = 1920 bytes
        //spr.setPivot(32, 55);      // Set pivot relative to top left corner of Sprite
        sprt.fillSprite(TFT_BLACK); // Fill the Sprite with black
	}
	
	void init()
	{
		valida=true;
		tft.init();
		tft.setRotation(3);
		tft.loadFont(AA_FONT_SMALL);	
		tft.setTextColor(TFT_WHITE, 0x5ACB);
		for(int i=0; i<10; i++)
		  {
			list_text[i]="";
		  }
		menu_level=0;
		loadDisplayFiles();
	}
	
	// Charge les fichiers d'organisation depuis la flash dans la RAM
	void loadDisplayFiles() {
		menutitleBuffer = loadFileToBuffer("/menutitle.txt", &menutitleSize);
		if (menutitleBuffer) {
		  Serial.printf("menutitle.txt chargé (%d octets)\n", menutitleSize);
		} else {
		  Serial.println("Échec du chargement de menutitle.txt");
		}

		menulistBuffer = loadFileToBuffer("/menulist.txt", &menulistSize);
		if (menulistBuffer) {
		  Serial.printf("menulist.txt chargé (%d octets)\n", menulistSize);
		} else {
		  Serial.println("Échec du chargement de menulist.txt");
		}
		
		menuorgBuffer = loadFileToBuffer("/menuorg.txt", &menuorgSize);
		if (menulistBuffer) {
		  Serial.printf("menuorg.txt chargé (%d octets)\n", menuorgSize);
		} else {
		  Serial.println("Échec du chargement de menuorg.txt");
		}
	}
	// Fonction utilitaire pour charger un fichier dans un buffer en RAM
	char* loadFileToBuffer(const char* filename, size_t* fileSize) {
		fs::File file = FFat.open(filename, "r");
		if (!file) {
		  Serial.printf("Impossible d'ouvrir %s\n", filename);
		  return NULL;
		}
		*fileSize = file.size();
		char* buffer = (char*)malloc(*fileSize + 1);
		if (buffer) {
		  file.readBytes(buffer, *fileSize);
		  buffer[*fileSize] = '\0'; // Terminaison chaîne
		} else {
		  Serial.printf("Erreur d'allocation pour %s\n", filename);
		}
		file.close();
		return buffer;
	}
	// Exemple de fonction qui extrait une ligne du buffer
	// lineIndex correspond au numéro de ligne désiré (0 pour la première)
	String getLineFromBuffer(const char* buffer, int lineIndex) {
		int currentLine = 0;
		const char* p = buffer;
		const char* lineStart = buffer;
		while (*p) {
		  if (*p == '\n' || *p == '\r') {
			// Si c'est la ligne recherchée, on la retourne
			if (currentLine == lineIndex) {
			  int len = p - lineStart;
			  return String(String(lineStart).substring(0, len));
			}
			// Passer aux lignes suivantes (ignorer séquences \n et \r consécutives)
			currentLine++;
			while (*p == '\n' || *p == '\r') {
			  p++;
			}
			lineStart = p;
		  } else {
			p++;
		  }
		}
		return "";
	}
	
	void display_top()
	{
		sprt.pushSprite(0, 0);
	}
	
	void display_bottom()
	{
		sprt.pushSprite(0, 200);
	}
	
	void display_window()
	{
		spr.pushSprite(0, 40);
		Serial.println("display_window");
	}
	
	void clear()
	{
		spr.fillSprite(TFT_BLACK);
		sprt.fillSprite(TFT_BLACK);
	}
	
	
	void display_compressor(float volaudio, float volcomp)
	{
		//Serial.println(volaudio);
		
		spr.fillRect(200, 20, 40, 105,  TFT_BLACK);
		spr.fillRect(90, 120-volaudio*100.0, 40, volaudio*100+5,  TFT_BLUE);
		spr.fillRect(140, 20, 40, (1.0-volcomp)*100.0+5,  TFT_RED);
	}
	
	void menu_top(int titlenum, int bcol=0x5ACB)
	{
		Serial.println("menu_top");
		sprt.setTextColor(TFT_WHITE);
		sprt.fillRect(0, 0, SCREEN_WIDTH, 40,  bcol);
		sprt.drawRect(0, 0, SCREEN_WIDTH, 40, 0x3186);
		
		if (menutitleBuffer == NULL) {
		  Serial.println("menutitleBuffer vide");
		  return;
		}
		int lineToRead = titlenum * 2;
		String titleSt = getLineFromBuffer(menutitleBuffer, lineToRead);
		Serial.println(titleSt);
		draw_title(titleSt);
	}
	
	int find_title_num() {
	  if (menutitleBuffer == NULL) {
		Serial.println("menutitleBuffer vide");
		return 0;
	  }
	  
	  int lineIndex = 0;
	  while (true) {
		String line = getLineFromBuffer(menutitleBuffer, lineIndex);
		line.trim();
		// On arrête la lecture si la ligne est vide ou si elle commence par un marqueur de fin (#)
		if (line.length() == 0 || line.startsWith("#")) {
		  break;
		}
		lineIndex++;
	  }
  // On considère que chaque titre occupe 2 lignes
  return lineIndex / 2;
}
	
	void menu_bottom(int selnum, int menuselect, int bcol=0x5ACB)
	{
		Serial.println("menu_bottom");
 		
		sprt.fillRect(0, 0, SCREEN_WIDTH, 40,  bcol);
		sprt.drawFastVLine(SCREEN_WIDTH/3, 0, 40, 0x3186);
		sprt.drawFastVLine(SCREEN_WIDTH*2/3, 0, 40, 0x3186);
		sprt.drawString("<", SCREEN_WIDTH/6, 10, 2);
		sprt.drawString(">", SCREEN_WIDTH*5/6, 10, 2);
		
		// Vérifier que le buffer des titres est chargé
		if (menutitleBuffer == NULL) {
			Serial.println("menutitleBuffer vide");
			return;
		}

		// Selon votre format, on saute 2*selnum lignes puis on lit la ligne de titre
		// suivie de la ligne de menu.
		int titleLineIndex = 2 * selnum;      // La ligne de titre à afficher
		int menuLineIndex  = titleLineIndex + 1; // La ligne suivante contient le code du menu

		String titleSt = getLineFromBuffer(menutitleBuffer, titleLineIndex);
		String menu    = getLineFromBuffer(menutitleBuffer, menuLineIndex);

		Serial.println("Titre : " + titleSt);
		Serial.println("Menu  : " + menu);

		// Tester le contenu de la chaîne 'menu'
		char Buf[20];
		menu.toCharArray(Buf, sizeof(Buf));

		if (Buf[0] == 'n' && Buf[1] == 'o') {
			Serial.println("no select");
		}
		else if (Buf[0] == 'r' && Buf[1] == 'e' && Buf[2] == 'c') {
			Serial.println("no select");
			draw_bottom_menu("Rec");
		}
		else if (Buf[0] == 'v' && Buf[1] == 'a' && Buf[2] == 'l') {
			Serial.println("validate");
			draw_bottom_menu("validate");
		}
		else if (Buf[0] == 'o' && Buf[1] == 'n' && Buf[2] == 'o') {
			if (menuselect == 0)  draw_bottom_menu("Off");
			else draw_bottom_menu("On");
		}
		else if (Buf[0] == 'o' && Buf[1] == 'k') {
				draw_bottom_menu("OK");
		}
		else {
			// Sinon, utiliser le buffer menulistBuffer pour obtenir la ligne détaillée.
			if (menulistBuffer == NULL) {
			  Serial.println("menulistBuffer vide");
			  return;
			}
			int lineIndex = 0;
			String line;
			char Buf2[20];
			bool markerFound = false;
			// Parcourir le buffer menulistBuffer pour trouver une ligne commençant par '/'
			// qui correspond aux deux premiers caractères de Buf.
			while (true) {
			  line = getLineFromBuffer(menulistBuffer, lineIndex);
			  if (line.length() == 0) break;
			  line.toCharArray(Buf2, sizeof(Buf2));
			  if (Buf2[0] == '/') {
				// Comparer les caractères 1 et 2 de Buf2 avec ceux de Buf
				if (Buf[0] == Buf2[1] && Buf[1] == Buf2[2]) {
				  markerFound = true;
				  break;
				}
			  }
			  lineIndex++;
			  if (lineIndex > 100) break; // sécurité
			}
			Serial.println("Marqueur trouvé à la ligne " + String(lineIndex));
			// Avancer de 'menuselect' lignes à partir du marqueur
			for (int i = 0; i < menuselect; i++) {
			  line = getLineFromBuffer(menulistBuffer, lineIndex);
			  lineIndex++;
			}
			line = getLineFromBuffer(menulistBuffer, lineIndex);
			Serial.println("Menu sélectionné : " + line);
			draw_bottom_menu(line);
		}
	}
	
	int list_bottom(int selnum, int listselnum, int bcol=0x5ACB)
	{
      int ret = 0;
	  Serial.println("list_bottom");

	  // On vérifie que le buffer préchargé pour les titres est disponible
	  if (menutitleBuffer == NULL) {
		Serial.println("menutitleBuffer vide");
		return ret;
	  }
	  
	  // Dans ce format, on suppose que chaque entrée occupe 2 lignes.
	  int skipLines = 2 * selnum;  // On saute 2*selnum lignes
	  String titleSt = getLineFromBuffer(menutitleBuffer, skipLines);
	  String menu    = getLineFromBuffer(menutitleBuffer, skipLines + 1);

	  char Buf[20];
	  menu.toCharArray(Buf, sizeof(Buf));
	  
	  if (Buf[0] == 'n' && Buf[1] == 'o') {
		Serial.println("no select");
	  } 
	  else if (Buf[0] == 'r' && Buf[1] == 'e' && Buf[2] == 'c') {
		Serial.println("no select");
		but_record();
	  } 
	  else {
		if (Buf[0] == 'v' && Buf[1] == 'a' && Buf[2] == 'l') {
		  Serial.println("validate");
		  draw_validate();
		  ret = -1;
		}
		else if (Buf[0] == 'o' && Buf[1] == 'k') {
		  Serial.println("OK");
		  ret = -3;
		}
		else if (Buf[0] == 'o' && Buf[1] == 'n' && Buf[2] == 'o') {
		  ret = -2;
		}
		else {
		  // Si aucun des cas ci-dessus ne s'applique, on utilise le buffer menulistBuffer.
		  if (menulistBuffer == NULL) {
			Serial.println("menulistBuffer vide");
			return ret;
		  }
		  // Recherche dans menulistBuffer du marqueur correspondant
		  int lineIndex = 0;
		  bool markerFound = false;
		  char Buf2[20];
		  while (lineIndex < 100 && !markerFound) {
			String markerLine = getLineFromBuffer(menulistBuffer, lineIndex);
			markerLine.toCharArray(Buf2, sizeof(Buf2));
			Serial.println(markerLine);
			if (Buf2[0] == '/') {
			  // Comparer les deux premiers caractères du menu avec ceux du marqueur (à partir de l'indice 1)
			  if (Buf[0] == Buf2[1] && Buf[1] == Buf2[2]) {
				markerFound = true;
				break;
			  }
			}
			lineIndex++;
		  }
		  
		  // Ensuite, à partir de ce marqueur, on lit les lignes de la liste
		  int i = 0;
		  while (true) {
			String line = getLineFromBuffer(menulistBuffer, lineIndex);
			line.toCharArray(Buf, sizeof(Buf));
			if (Buf[0] == '/') {  // Arrêt si on rencontre un nouveau marqueur
			  break;
			}
			Serial.println(i);
			list_text[i] = line;
			Serial.println(list_text[i]);
			i++;
			lineIndex++;
			if (i >= 20) break; // Limite du tableau list_text
		  }
		  list_size = i - 1;
		  ret = list_size - 1;
		  
		  display_list(listselnum, 0, 10, 100);
		}
	  }
	  return ret;
	}
	
	void display_array(String array[],int numwifi, int index)
	{
		Serial.println("display_array");
		spr.fillRect(0, 10+20*index, 160, 20, TFT_RED);

		Serial.println(numwifi);
		for(int i=0; i<numwifi; i++) 
		{
			//spr.drawString(array[i], 30, 10+i*20, 2);
			draw_free(30, 10+i*20, array[i], SMALLCHAR);
		}
	}
	
	String display_fatfiles(int index)
	{
		Serial.println("display_fatfiles");
		
		String filelist[50];
		fs::File root = FFat.open("/");
		fs::File file = root.openNextFile();
		int filenumber=0;
		
		while(file){
			if(!file.isDirectory()){
				String filetemp=file.name();
				if (filetemp.endsWith(".wav"))
				{
					filelist[filenumber]=filetemp;
					Serial.println(filelist[filenumber]);
					filenumber++;
				}
			}
			file = root.openNextFile();
			
		}
		if(index>filenumber-1) index=filenumber-1;
		int page = index/7;
		int place = index%7;
		spr.fillRect(0, 10+20*place, 20, 20, TFT_RED);
		Serial.println(filenumber);
		int endfile;
		if(filenumber<(page+1)*7) endfile = filenumber;
		else endfile = (page+1)*7;
		for(int i=page*7; i<endfile; i++) 
		{
			draw_free(30, 10+(i-page*7)*20, filelist[i], SMALLCHAR);
			//spr.drawString(filelist[i], 30, 10+(i-page*7)*20, 2);
		}
		file.close();
		root.close();
			
		spr.fillRect(200, 50, 70, 60,  0x5ACB);
		draw_free(210, 60, "press", SMALLCHAR);
		draw_free(210, 80, "to load", SMALLCHAR);
		return filelist[index];
	}
	
	void draw_validate()
	{
		Serial.println("draw_validate");
		spr.fillRect(80, 80, 120, 20, 0xF800);
		spr.drawRect(80, 80, 120, 20, TFT_WHITE);
		spr.setTextColor(TFT_WHITE);
		draw_free(110, 82, "Are you sure ?", SMALLCHAR);
		//tft.drawString("Are you sure ?", 110, 82, 2);
		
		if(valida) spr.fillRect(80, 100, 60, 20, 0x0A80);
		else spr.fillRect(80, 100, 60, 20, 0xF800);
		spr.drawRect(80, 100, 60, 20, TFT_WHITE);
		spr.setTextColor(TFT_WHITE);
		draw_free(100, 102, "Yes", SMALLCHAR);
		//tft.drawString("Yes", 100, 102, 2);
		if(valida) spr.fillRect(140, 100, 60, 20, 0xF800);
		else spr.fillRect(140, 100, 60, 20, 0x0A80);
		spr.drawRect(140, 100, 60, 20, TFT_WHITE);
		spr.setTextColor(TFT_WHITE);
		draw_free(160, 102, "No", SMALLCHAR);
		//tft.drawString("No", 160, 102, 2);
		display_window();
	}
	
	void change_menu_select(int _sens)
	{
		Serial.println("change_menu_select");
		line_selected[menu_level]+=_sens;
		line_selected[menu_level+1]=0;
		if(line_selected[menu_level]>line_selected_max[menu_level]) line_selected[menu_level]=line_selected_max[menu_level];
		if(line_selected[menu_level]<0) line_selected[menu_level]=0;
		if(menu_level==0) display_list(line_selected[0],0,10,80);
		if(menu_level==1) display_list(line_selected[1],80,10,65);
		if(menu_level==2) display_list(line_selected[2],145,10,65);
		if(menu_level==3) display_list(line_selected[3],210,10,75);
	}
	
	int search_menutitle(String stringsearch)
	{
	  Serial.println("search_menutitle");
	  Serial.println(stringsearch);
	  
	  // Vérifier que le buffer est chargé
	  if (menutitleBuffer == NULL) {
		Serial.println("menutitleBuffer vide");
		return 200;
	  }
	  
	  int lineIndex = 0;
	  int ret = 200;
	  
	  while (true) {
		// Extraire la ligne d'indice lineIndex depuis le buffer préchargé
		String line = getLineFromBuffer(menutitleBuffer, lineIndex);
		line.trim();  // Supprimer espaces et retours en début/fin
		if (line.length() == 0 || line.startsWith("#")) {
		  // Fin du contenu utile
		  break;
		}
		
		// Vérifier uniquement sur les lignes paires (titres)
		if (lineIndex % 2 == 0) {
		  Serial.println("Comparaison avec : " + line);
		  if (line.equals(stringsearch)) {
			ret = lineIndex / 2;
			break;
		  }
		}
		lineIndex++;
	  }
	  
	  return ret;
	}
	
	int menu_right() {
	  Serial.println("menu_right");
	  int ret = 200;
	  char Buf[20];
	  list_text[line_selected[menu_level]].toCharArray(Buf, sizeof(Buf));
	  
	  // Si la ligne commence par '@', on recherche un titre dans le buffer de menutitle
	  if (Buf[0] == '@') {
		String search = list_previous + " " + list_text[line_selected[menu_level]].substring(1);
		Serial.println(search);
		ret = search_menutitle(search);
	  }
	  else {
		// Sinon, on sauvegarde la ligne actuelle et on passe au niveau suivant
		list_previous = list_text[line_selected[menu_level]];
		menu_level++;
		// Réinitialise la sélection pour le nouveau niveau
		line_selected[menu_level] = 0;
		menu_hierarchy();  // Mise à jour de la liste du nouveau niveau
	  }
	  return ret;
	}

	// Cette fonction extrait et affiche la colonne de menu pour un niveau donné,
	// en ne retenant que les lignes qui sont des enfants du parent sélectionné.
	void populateMenuColumn(int lvl) {
	  if (menuorgBuffer == NULL) {
		Serial.println("menuorgBuffer vide");
		return;
	  }

	  // Définir le préfixe attendu pour ce niveau
	  char expectedPrefix;
	  switch (lvl) {
		case 0: expectedPrefix = '-'; break;
		case 1: expectedPrefix = '/'; break;
		case 2: expectedPrefix = '*'; break;
		case 3: expectedPrefix = '+'; break;
		default: expectedPrefix = ' '; break;
	  }
	  
	  String tempList[20]; // Tableau temporaire pour stocker les lignes de ce niveau
	  int count = 0;       // Nombre de lignes trouvées
	  
	  // Si le niveau est 0, on parcourt tout le buffer et on récupère toutes les lignes qui commencent par '-'
	  if (lvl == 0) {
		int lineIndex = 0;
		while (true) {
		  String line = getLineFromBuffer(menuorgBuffer, lineIndex);
		  if (line.length() == 0 || line.startsWith("#")) break;
		  line.trim();
		  if (line.charAt(0) == expectedPrefix) {
			// On retire le premier caractère (pour le niveau 0, lvl+1 = 1)
			String displayLine = (line.length() > 1) ? line.substring(1) : "";
			tempList[count++] = displayLine;
			if (count >= 20) break;
		  }
		  lineIndex++;
		}
	  }
	  else {
		// Pour lvl >= 1, on doit d'abord localiser le parent dans le buffer.
		// Le parent correspond au niveau précédent (lvl - 1) dont le préfixe est défini ci-dessous :
		char parentPrefix;
		switch (lvl - 1) {
		  case 0: parentPrefix = '-'; break;
		  case 1: parentPrefix = '/'; break;
		  case 2: parentPrefix = '*'; break;
		  case 3: parentPrefix = '+'; break;
		  default: parentPrefix = ' '; break;
		}
		
		// Le numéro de sélection du parent est stocké dans line_selected[lvl - 1].
		int parentSel = line_selected[lvl - 1];
		int parentCount = 0;
		int lineIndex = 0;
		int parentStartLine = -1;
		
		// Rechercher la ligne du parent sélectionné
		while (true) {
		  String line = getLineFromBuffer(menuorgBuffer, lineIndex);
		  if (line.length() == 0 || line.startsWith("#")) break;
		  line.trim();
		  if (line.charAt(0) == parentPrefix) {
			if (parentCount == parentSel) {
			  parentStartLine = lineIndex;
			  break;
			}
			parentCount++;
		  }
		  lineIndex++;
		}
		
		// Si le parent a été trouvé, on lit à partir de parentStartLine+1
		if (parentStartLine >= 0) {
		  lineIndex = parentStartLine + 1;
		  while (true) {
			String line = getLineFromBuffer(menuorgBuffer, lineIndex);
			if (line.length() == 0 || line.startsWith("#")) break;
			line.trim();
			// Si on rencontre une ligne qui commence par le parentPrefix ou un marqueur de niveau supérieur,
			// cela signifie qu'on est sorti du bloc enfant du parent.
			if (line.charAt(0) == parentPrefix) break;
			// Si la ligne a le préfixe attendu pour le niveau courant, on la conserve.
			if (line.charAt(0) == expectedPrefix) {
			  // Pour enlever le préfixe hiérarchique, on retire (lvl+1) caractères.
			  int removeCount = lvl + 1;
			  String displayLine = (line.length() > removeCount) ? line.substring(removeCount) : "";
			  tempList[count++] = displayLine;
			  if (count >= 20) break;
			}
			lineIndex++;
		  }
		}
	  }
	  
	  // Mise à jour des variables globales pour ce niveau
	  list_size = count;
	  line_selected_max[lvl] = (count > 0) ? count - 1 : 0;
	  // Copier dans le tableau global list_text (pour affichage)
	  for (int i = 0; i < count; i++) {
		list_text[i] = tempList[i];
	  }
	  
	  // Choix des coordonnées d'affichage pour ce niveau
	  int x, y, h;
	  switch (lvl) {
		case 0: x = 0;   y = 10; h = 80;  break;
		case 1: x = 80;  y = 10; h = 65;  break;
		case 2: x = 145; y = 10; h = 65;  break;
		case 3: x = 210; y = 10; h = 75;  break;
		default: x = 0;  y = 10; h = 80;  break;
	  }
	  
	  // Affichage de la colonne avec la fonction existante
	  display_list(line_selected[lvl], x, y, h);
	}

	// La fonction menu_hierarchy() affiche systématiquement les colonnes des niveaux 0 jusqu'au niveau actif.
	// Pour le niveau actif, la colonne ne contiendra que les options enfants du parent sélectionné.
	void menu_hierarchy() {
	  if (menuorgBuffer == NULL) {
		Serial.println("menuorgBuffer vide");
		return;
	  }
	  
	  // Pour chaque niveau allant de 0 jusqu'au niveau actif, on affiche la colonne correspondante.
	  for (int lvl = 0; lvl <= menu_level; lvl++) {
		populateMenuColumn(lvl);
	  }
	}

	
	void cpu_usage(int cpu)
	{
		String aff = String(cpu) + " %";
        if(cpu<80) tft.fillRect(SCREEN_WIDTH/2+90, 0, 50, 39,  TFT_BLUE);
		else tft.fillRect(SCREEN_WIDTH/2+90, 0, 50, 39,  TFT_RED);
        //tft.drawString(aff, SCREEN_WIDTH/2+100, 10, 2);
		
		tft.setCursor(SCREEN_WIDTH/2+100, 10);	
		tft.println(aff);
	}
	
	void tempo_on()
	{
		tft.fillRect(250, 10, 20, 20, 0x5ACB);
	}
	
	void tempo_sync()
	{
		tft.fillRect(250, 10, 20, 20, TFT_BLUE);
	}
	
	void draw_warning(String txt)
	{
		spr.fillRect(60, 30, 160, 100, TFT_RED);
		spr.drawRect(60,30,160,100, TFT_WHITE);
		draw_centered(70, txt, SMALLCHAR);
	    //spr.drawString(txt, SCREEN_WIDTH/2-20, 70, 4);
	}
	
	void draw_centered(int height, String txt, int size)
	{
		//if(size==BIGCHAR) spr.drawString(txt, SCREEN_WIDTH/2-txt.length()*7-3, height, size);
		//if(size==SMALLCHAR) spr.drawString(txt, SCREEN_WIDTH/2-txt.length()*4, height, size);
		spr.setTextColor(TFT_WHITE, TFT_BLACK);
		if(size==BIGCHAR)
		{
			spr.loadFont(AA_FONT_HUGE);
			int textWidth = spr.textWidth(txt);
			spr.setCursor(SCREEN_WIDTH/2-textWidth/2, height);	
		}
		if(size==SMALLCHAR)
		{
			spr.loadFont(AA_FONT_SMALL);
			int textWidth = spr.textWidth(txt);
			spr.setCursor(SCREEN_WIDTH/2-textWidth/2, height);
		}
		spr.println(txt);
	}
	
	void draw_string_number_center_red(String txt, int num)
	{
		String aff = String(num);
		aff = txt + aff;
		spr.setTextColor(TFT_RED, TFT_BLACK);
		spr.loadFont(AA_FONT_HUGE);
		int textWidth = spr.textWidth(aff);
		spr.setCursor(SCREEN_WIDTH/2-textWidth/2, 60);
		spr.println(aff);
	}
	
	void draw_free(int absc, int height, String txt, int size)
	{
		//if(size==BIGCHAR) spr.drawString(txt, SCREEN_WIDTH/2-txt.length()*7-3, height, size);
		//if(size==SMALLCHAR) spr.drawString(txt, SCREEN_WIDTH/2-txt.length()*4, height, size);
		spr.setTextColor(TFT_WHITE, TFT_BLACK);
		if(size==BIGCHAR)
		{
			spr.loadFont(AA_FONT_HUGE);
			spr.setCursor(absc, height);	
		}
		if(size==SMALLCHAR)
		{
			spr.loadFont(AA_FONT_SMALL);
			spr.setCursor(absc, height);
		}
		spr.println(txt);
	}
	
	void draw_title(String txt)
	{
		sprt.loadFont(AA_FONT_LARGE);
		sprt.setCursor(SCREEN_WIDTH/2-txt.length()*6, 10);	
		sprt.setTextColor(TFT_WHITE, 0x5ACB);
		sprt.println(txt);
		//sprt.drawString(txt, SCREEN_WIDTH/2-txt.length()*7, 10, BIGCHAR);
	}
	
	void draw_bottom_menu(String txt)
	{
		sprt.loadFont(AA_FONT_SMALL);
		sprt.setCursor(SCREEN_WIDTH/2-txt.length()*4, 10);	
		sprt.setTextColor(TFT_WHITE, 0x5ACB);
		sprt.println(txt);
		//sprt.drawString(txt, SCREEN_WIDTH/2-txt.length()*4, 10, SMALLCHAR);
	}
	
	void draw_string_center(String txt)
	{
		draw_centered(60, txt, BIGCHAR);
	}
	
	void draw_string_number_center(String txt, int num)
	{
		String aff = String(num);
		aff = txt + aff;
		draw_centered(60, aff, BIGCHAR);
	}
	
	void draw_number_string_center(int num, String txt)
	{
		String aff = String(num);
		aff = aff+txt;
		draw_centered(60, aff, BIGCHAR);
	}
	
	void draw_number_string_low(int num, String txt)
	{
		String aff = String(num);
		aff = aff+txt;
		draw_centered(140, aff, SMALLCHAR);
	}
	
	void midi_learn()
	{
		Serial.println("midi_learn");
	  spr.fillRect(30, 20, 220, 110, TFT_BLUE);
	  draw_centered(60, "MIDI LEARN", BIGCHAR);
	  spr.fillRect(0, 130, 280, 30, TFT_BLACK);
	  draw_centered(140, "Turn controler or Click encoder", SMALLCHAR);
	  //spr.drawString("MIDI LEARN", 30, 70, 4);
	}
	
	void midi_learned(int num)
	{
		Serial.println("midi_learned");
	  spr.fillRect(30, 20, 220, 110, TFT_BLUE);
	  String aff = String(num);
		aff = "CC " + aff;
	  //spr.drawString(aff, SCREEN_WIDTH/2-75, 70, 4);
	  draw_centered(60, aff, BIGCHAR);
	  spr.fillRect(0, 130, 280, 30, TFT_BLACK);
	  draw_centered(140, "Click encoder to quit learn mode", SMALLCHAR);
	  Serial.println("end midi_learned");
	}

	
	void draw_adsr(int valA, int valD, int valS, int valR, int part)
	{
	  int color;
  
	  if(part==0) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(10, 150, 10+valA/2, 20, color);
	  if(part==1) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(10+valA/2, 20, 10+valA/2+valD/2, 147-valS, color);
	  if(part==2) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(10+valA/2+valD/2, 147-valS, 220, 147-valS, color);
	  if(part==3) color=TFT_BLUE;
	  else color=0xF808;
	  spr.drawLine(220, 147-valS, 220+valR/2 , 150, color);

	  spr.drawCircle(10, 150, 2, TFT_WHITE);
	  spr.drawCircle(10+valA/2, 20, 2, TFT_WHITE);
	  spr.drawCircle(10+valA/2+valD/2, 147-valS, 2, TFT_WHITE);
	  spr.drawCircle(220, 147-valS, 2, TFT_WHITE);
	  spr.drawCircle(220+valR/2, 150, 2, TFT_WHITE);
	}
	
	void draw_waveform_lfo(float wave[], float vol, float cutoff)
	{
		Serial.println("draw lfo");
		int borne=1024/5;
		Serial.println(borne);
		Serial.println(wave[100]);
      for(int i=0; i<borne-1; i++)
      {
        int point1=160 - (wave[i*5]*vol)*80.0-cutoff*80;
        int point2=160 - (wave[i*5+3]*vol)*80.0-cutoff*80;
        if(point1<80) point1=80;
        if(point2<80) point2=80;
        if(point1>160) point1=160;
        if(point2>160) point2=160;
        if(i==0) point1=120;
        if(i==199) point2=120;
        int dist = point2-point1;
 
        spr.drawFastVLine(40+i,point1-40 ,160-point1, TFT_DARKGREEN);

        if(dist>0) {
          spr.drawFastVLine(40+i,point1-40 ,dist, TFT_GREEN);   
        }
        if(dist<0) {
          spr.drawFastVLine(40+i,point2-40 ,-dist, TFT_GREEN);
        }
        if(dist==0) spr.drawFastVLine(40+i,point1-40 ,1, TFT_GREEN);
      }
      //tft.drawFastHLine(40, 120,200, TFT_GREEN);
      spr.drawFastVLine(40, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(140, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(240, 40 , 80, TFT_DARKGREY);
	}
	
	void draw_waveform(float *wavefo)
	{
		int borne=SAMPLES_PER_CYCLE/5;
		Serial.println("draw_waveform");
		Serial.println(borne);
      for(int i=0; i<borne-1; i++)
      {
        //int point1=32 - sine[i<<4]*28;
        //int point2=32 - sine[(i+1)<<4]*28;
        int point1=120 - *(wavefo+(i*5))*80;
        int point2=120 - *(wavefo+((i+1)*5))*80;
        if(i==0) point1=120;
        if(i==borne-2) point2=120;
        int dist = point2-point1;
        /*if(dist>0) tft.drawFastVLine(point1, 32+i,dist, TFT_WHITE);
        if(dist<0) tft.drawFastVLine(point2, 32+i,-dist, TFT_WHITE);
        if(dist==0) tft.drawFastVLine(point1, 32+i,1, TFT_WHITE);*/
        if(point1<120)
        {
          spr.drawFastVLine(40+i,point1-40 ,120-point1, TFT_DARKGREEN);
        }
        else
        {
          spr.drawFastVLine(40+i,80 ,point1-120, TFT_DARKGREEN);
        }
        if(dist>0) {
          spr.drawFastVLine(40+i,point1-40 ,dist, TFT_GREEN);   
        }
        if(dist<0) {
          spr.drawFastVLine(40+i,point2-40 ,-dist, TFT_GREEN);
          //tft.drawFastVLine(40+i,point1 ,110-point1, 0x00B0);
        }
        if(dist==0) spr.drawFastVLine(40+i,point1-40 ,1, TFT_GREEN);
      }
      //tft.drawFastHLine(40, 120,200, TFT_GREEN);
      spr.drawFastVLine(40, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(borne/2+39, 40 , 80, TFT_DARKGREY);
      spr.drawFastVLine(borne+40, 40 , 80, TFT_DARKGREY);
	}
	
	void draw_wave(int16_t *wavefile, int startind, int samplength, int sampstart, int sampend)
	{
		Serial.println("draw_wave");
		int indinc = samplength/200;
		int borne=200;
		spr.fillRect(40+sampstart*200/127, 20, borne-(sampend+sampstart)*200/127, 120,  TFT_DARKGREY);
      for(int i=0; i<200; i++)
      {
        //int point1=32 - sine[i<<4]*28;
        //int point2=32 - sine[(i+1)<<4]*28;
        int point1=120 - *(wavefile+startind+(i*indinc))/512;
        int point2=120 - *(wavefile+startind+((i+1)*indinc))/512;
        if(i==0) point1=120;
        if(i==borne-2) point2=120;
        int dist = point2-point1;
        /*if(dist>0) tft.drawFastVLine(point1, 32+i,dist, TFT_WHITE);
        if(dist<0) tft.drawFastVLine(point2, 32+i,-dist, TFT_WHITE);
        if(dist==0) tft.drawFastVLine(point1, 32+i,1, TFT_WHITE);*/
        if(point1<120)
        {
          spr.drawFastVLine(40+i,point1-40 ,120-point1, TFT_DARKGREEN);
        }
        else
        {
          spr.drawFastVLine(40+i,80 ,point1-120, TFT_DARKGREEN);
        }
        if(dist>0) {
          spr.drawFastVLine(40+i,point1-40 ,dist, TFT_GREEN);   
        }
        if(dist<0) {
          spr.drawFastVLine(40+i,point2-40 ,-dist, TFT_GREEN);
          //tft.drawFastVLine(40+i,point1 ,110-point1, 0x00B0);
        }
        if(dist==0) spr.drawFastVLine(40+i,point1-40 ,1, TFT_GREEN);
      }
      //tft.drawFastHLine(40, 120,200, TFT_GREEN);
      spr.drawFastVLine(40+sampstart*200/127, 20 , 120, TFT_RED);
      spr.drawFastVLine(borne+40-sampend*200/127, 20 , 120, TFT_RED);
	}

#ifndef HAMMOND	
	void draw_waveform_osc(OscMonoPoly os, int num)
    {
      int borne=SAMPLES_PER_CYCLE/5;
      //Serial.println("draw_waveform_osc");
      //Serial.println(os.pwm_conv[num]);
      if(os.pwm_conv[num]!=0)
      {
        //Serial.println(((os.fact1+os.fact2/(300+os.fl))*300));
		  for(int i=0; i<borne-1; i++)
		  {
			int i1,i2;
			if(os.pwm_conv[num]>64) 
			 {
			   i1 = (os.fact1[num]-os.fact2[num]/(SAMPLES_PER_CYCLE-i*5+os.fl[num]))*i*5;
			   i2= (os.fact1[num]-os.fact2[num]/(SAMPLES_PER_CYCLE-i*5-5 +os.fl[num]))*(i*5+5);
			 }
			 else 
			 {
			   i1 = (os.fact1[num]+os.fact2[num]/(i*5+os.fl[num]))*i*5;
			   i2 = (os.fact1[num]+os.fact2[num]/(i*5+5+os.fl[num]))*(i*5+5);
			 }
			//int point1=32 - sine[i<<4]*28;
			//int point2=32 - sine[(i+1)<<4]*28;
			int point1=120 - os.wave[num][i1]*80;
			int point2=120 - os.wave[num][i2]*80;
			if(i==0) point1=120;
			if(i==borne-2) point2=120;
			int dist = point2-point1;
			/*if(dist>0) tft.drawFastVLine(point1, 32+i,dist, TFT_WHITE);
			if(dist<0) tft.drawFastVLine(point2, 32+i,-dist, TFT_WHITE);
			if(dist==0) tft.drawFastVLine(point1, 32+i,1, TFT_WHITE);*/
			if(point1<120)
			{
			  spr.drawFastVLine(40+i,point1-40 ,120-point1, TFT_DARKGREEN);
			}
			else
			{
			  spr.drawFastVLine(40+i,80 ,point1-120, TFT_DARKGREEN);
			}
			if(dist>0) {
			  spr.drawFastVLine(40+i,point1-40 ,dist, TFT_GREEN);   
			}
			if(dist<0) {
			  spr.drawFastVLine(40+i,point2-40 ,-dist, TFT_GREEN);
			  //tft.drawFastVLine(40+i,point1 ,120-point1, 0x00B0);
			}
			if(dist==0) spr.drawFastVLine(40+i,point1-40 ,1, TFT_GREEN);
			
			if(point1>=120 && dist>=0) {
			  spr.drawFastVLine(40+i,point2+dist-39 ,180-point2-dist-1, TFT_BLACK);   
			}
			if(point1>=120 && dist<0) {
			  spr.drawFastVLine(40+i,point1+dist-39 ,180-point1-dist-1, TFT_BLACK);  
			}
			if(point1<=120 && dist>=0) {
			  spr.drawFastVLine(40+i,10 ,point1-50, TFT_BLACK);
			}
			if(point1<=120 && dist<0) {
			  spr.drawFastVLine(40+i,10 ,point2-50, TFT_BLACK);
			}

			if(point1>=120 && point2>=120) {
			  spr.drawFastVLine(40+i,30 ,50, TFT_BLACK);
			}
			if(point1<=120 && point2<=120) {
			  spr.drawFastVLine(40+i,81 ,50, TFT_BLACK);
			}
		  }
		  //tft.drawFastHLine(40, 120,200, TFT_GREEN);
		  spr.drawFastVLine(40, 40 , 80, TFT_DARKGREY);
		  spr.drawFastVLine(39+borne/2, 40 , 80, TFT_DARKGREY);
		  spr.drawFastVLine(40+borne, 40 , 80, TFT_DARKGREY);
		}
		else draw_waveform(os.wave[num]);
	}
#endif

	void drawArc(int cx, int cy, int radius, float startAngle, float endAngle) {
	  // Définir le pas en radians pour obtenir un arc lisse
	  float step = 0.01; 
	  // Calculer le premier point de l'arc
	  int prevX = cx + radius * cos(startAngle);
	  int prevY = cy + radius * sin(startAngle);
	  
	  // Parcourir l'arc et tracer des segments entre points successifs
	  for (float angle = startAngle + step; angle <= endAngle; angle += step) {
		int x = cx + radius * cos(angle);
		int y = cy + radius * sin(angle);
		spr.drawLine(prevX, prevY, x, y,TFT_GREEN);
		prevX = x;
		prevY = y;
	  }
	}
	
	void draw_filter(float freq, float res, float mode)
	{
		for(int i=1; i<10; i++)
		{
		  spr.drawFastVLine(log10(100*i)*80-120,40,100,TFT_DARKGREY);
		  spr.drawFastVLine(log10(1000*i)*80-120,40,100,TFT_DARKGREY);
		}
		spr.drawFastVLine(log10(10000)*80-120,40,100,TFT_DARKGREY);
		spr.drawFastVLine(log10(20000)*80-120,40,100,TFT_DARKGREY);
		draw_free(30, 140, "100 Hz", SMALLCHAR);
		//spr.drawString("100 Hz", 30, 140, SMALLCHAR);
		draw_free(100, 140, "1000", SMALLCHAR);
		//spr.drawString("1000", 100, 140, SMALLCHAR);
		draw_free(180, 140, "10000", SMALLCHAR);
		int startx=10;
		int starty=40;
		int r1=50;
		int r2=8;
		float res2=res/2;

		int pente=20;

		int longu=100;
		float freqcal = log10(freq)*80-160-r1*sin(res2*0.0174); 
		if(mode==1) freqcal = log10(freq)*80-160-r1*sin(res2*0.0174); 
		if(mode==2) freqcal = 200-(log10(freq)*80-160)-r1*sin(res2*0.0174); 
		
		float C1x=startx+freqcal;
        float C1y= starty-r1;
		
		float C2x=C1x+(r1+r2)*sin(res2*0.0174);
		float C2y=C1y+cos(res2*0.0174)*(r1+r2);
		
        float Px=C2x+cos(-pente*0.0174)*r2;
		float Py=C2y+sin(-pente*0.0174)*r2;
 
		float P2x=Px+sin(pente*0.0174)*longu;
		float P2y=Py+cos(pente*0.0174)*longu+(1-cos(res2*0.0174))*r1;
 
		if(mode==1)
		{
			spr.drawFastHLine(startx,starty,freqcal,TFT_GREEN);
			spr.drawLine(Px, Py, P2x,P2y,TFT_GREEN);
			float startAngle = (90 - res2) * DEG_TO_RAD; // Conversion en radians
			float endAngle   = 90 * DEG_TO_RAD;            // Conversion en radians
			drawArc(C1x, C1y,r1,startAngle,endAngle);
			startAngle = (-90-res2) * DEG_TO_RAD; // Conversion en radians
			endAngle   = -pente * DEG_TO_RAD;
			drawArc(C2x, C2y, r2,startAngle,endAngle);
		}
		if(mode==2)
		{
			spr.drawFastHLine(280-startx-freqcal,starty,280-startx,TFT_GREEN);
			spr.drawLine(280-Px, Py, 280-P2x,P2y,TFT_GREEN);
		}	
		if(mode==3)
		{
			spr.drawLine(Px, Py, P2x,P2y,TFT_GREEN);
			spr.drawLine(Px-2*r2, Py, 2*Px-P2x-2*r2,P2y,TFT_GREEN);
		}
		spr.drawFastVLine(40, 40 , 60, TFT_DARKGREY);
	    spr.drawFastHLine(40, 100 , 200, TFT_DARKGREY);
	}
	
	void display_list(int ls, int x, int y, int larg)
	{
		Serial.println("display_list");
		Serial.println(ls);
		Serial.println(x);
		Serial.println(list_size);
		Serial.println(list_text[0]);
	  int max_line = 8;
	  int start_line=0;
	  if(ls>(max_line-1)) start_line=ls-max_line+1;
	  Serial.println(start_line);
	  
	  //uint8_t a = detect_button();
	  
	  if(list_size > max_line)
	  {
		spr.fillRect(x+larg/2-10, y+20*max_line+2, 20, 20, 0x5ACB);
		spr.fillRect(x+larg/2-10, y-12, 20, 20, TFT_BLACK);
	  }
	  
	  for(int i=0; i<list_size && i<max_line; i++)
	  {
		uint16_t line_color = 0xF800;
		if(i==(ls-start_line)) line_color = 0x0A80;
		spr.fillRect(x, y+i*20, larg, 20, line_color);
		spr.drawRect(x, y+i*20, larg, 20, TFT_WHITE);
		//spr.setTextColor(TFT_WHITE);
		//tft.drawString(list_text[i+start_line], x+5, y+2+i*20, 2);
		//tft.drawNumber(a, 50, 32+i*20,2);
		draw_free(x+5, y+2+i*20, list_text[i+start_line], SMALLCHAR);
	  }
	  
	  if((list_size-start_line) > max_line)
	  {
		spr.fillTriangle(x+larg/2-10,y+20*max_line+2, x+larg/2+10,y+20*max_line+2, x+larg/2,y+20*max_line+12,TFT_RED);
		spr.drawTriangle(x+larg/2-10,y+20*max_line+2, x+larg/2+10,y+20*max_line+2, x+larg/2,y+20*max_line+12,TFT_WHITE);
	  }
	  if(start_line>0)
	  {
		spr.fillTriangle(x+larg/2-10,y-2, x+larg/2+10,y-2, x+larg/2,y-12,TFT_RED);
		spr.drawTriangle(x+larg/2-10,y-2, x+larg/2+10,y-2, x+larg/2,y-12,TFT_WHITE);
	  }
	  display_window();
	}
	
	void drawBmp(const char *filename, int16_t x, int16_t y) {

	  if ((x >= tft.width()) || (y >= tft.height())) return;

	  fs::File bmpFS;

	  // Open requested file on SD card
	  bmpFS = FFat.open(filename, "r");

	  if (!bmpFS)
	  {
		Serial.print("File not found");
		return;
	  }

	  uint32_t seekOffset;
	  uint16_t w, h, row, col;
	  uint8_t  r, g, b;

	  uint32_t startTime = millis();

	  uint16_t fir = read16(bmpFS);
	  Serial.println(fir&0x000F);
	  Serial.println((fir&0x00F0)>>4);
	  Serial.println((fir&0x0F00)>>8);
	  Serial.println((fir&0xF000)>>12);

	  //if (fir == 0x4D42)
	  //{
		Serial.println("OK");
		read32(bmpFS);
		read32(bmpFS);
		seekOffset = read32(bmpFS);
		read32(bmpFS);
		w = read32(bmpFS);
		h = read32(bmpFS);

		if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
		{
		  y += h - 1;

		  bool oldSwapBytes = tft.getSwapBytes();
		  tft.setSwapBytes(true);
		  bmpFS.seek(seekOffset);

		  uint16_t padding = (4 - ((w * 3) & 3)) & 3;
		  uint8_t lineBuffer[w * 3 + padding];

		  for (row = 0; row < h; row++) {
			
			bmpFS.read(lineBuffer, sizeof(lineBuffer));
			uint8_t*  bptr = lineBuffer;
			uint16_t* tptr = (uint16_t*)lineBuffer;
			// Convert 24 to 16 bit colours
			for (uint16_t col = 0; col < w; col++)
			{
			  b = *bptr++;
			  g = *bptr++;
			  r = *bptr++;
			  *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
			}

			// Push the pixel row to screen, pushImage will crop the line if needed
			// y is decremented as the BMP image is drawn bottom up
			tft.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
		  }
		  tft.setSwapBytes(oldSwapBytes);
		  Serial.print("Loaded in "); Serial.print(millis() - startTime);
		  Serial.println(" ms");
		//}
		//else Serial.println("BMP format not recognized.");
	  }
	  bmpFS.close();
	}
	
	void drawBmpParam(const char *filename, int16_t x, int16_t y) {

	  if ((x >= spr.width()) || (y >= spr.height())) return;

	  fs::File bmpFS;

	  // Open requested file on SD card
	  bmpFS = FFat.open(filename, "r");

	  if (!bmpFS)
	  {
		Serial.println("File not found");
		return;
	  }

	  uint32_t seekOffset;
	  uint16_t w, h, row, col;
	  uint8_t  r, g, b;

	  uint32_t startTime = millis();

	  uint16_t fir = read16(bmpFS);
	  Serial.println(fir&0x000F);
	  Serial.println((fir&0x00F0)>>4);
	  Serial.println((fir&0x0F00)>>8);
	  Serial.println((fir&0xF000)>>12);

	  //if (fir == 0x4D42)
	  //{
		Serial.println("length width : ");
		read32(bmpFS);
		read32(bmpFS);
		seekOffset = read32(bmpFS);
		read32(bmpFS);
		w = read32(bmpFS);
		h = read32(bmpFS);
		Serial.println(w);
		Serial.println(h);

		if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
		{
		  y += h - 1;

		  bool oldSwapBytes = spr.getSwapBytes();
		  spr.setSwapBytes(true);
		  bmpFS.seek(seekOffset);

		  uint16_t padding = (4 - ((w * 3) & 3)) & 3;
		  uint8_t lineBuffer[w * 3 + padding];

		  for (row = 0; row < h; row++) {
			
			bmpFS.read(lineBuffer, sizeof(lineBuffer));
			uint8_t*  bptr = lineBuffer;
			uint16_t* tptr = (uint16_t*)lineBuffer;
			// Convert 24 to 16 bit colours
			for (uint16_t col = 0; col < w; col++)
			{
			  b = *bptr++;
			  g = *bptr++;
			  r = *bptr++;
			  *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
			}

			// Push the pixel row to screen, pushImage will crop the line if needed
			// y is decremented as the BMP image is drawn bottom up
			spr.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
		  }
		  spr.setSwapBytes(oldSwapBytes);
		  Serial.print("Loaded in "); Serial.print(millis() - startTime);
		  Serial.println(" ms");
		//}
		//else Serial.println("BMP format not recognized.");
	  }
	  bmpFS.close();
	}
	
	void drawBmpParam2(const char *filename, int16_t x, int16_t y) {

	  if ((x >= tft.width()) || (y >= tft.height())) return;

	  fs::File bmpFS;

	  // Open requested file on SD card
	  bmpFS = FFat.open(filename, "r");

	  if (!bmpFS)
	  {
		Serial.println("File not found");
		return;
	  }

	  uint32_t seekOffset;
	  uint16_t w, h, row, col;
	  uint8_t  r, g, b;

	  uint32_t startTime = millis();

	  uint16_t fir = read16(bmpFS);
	  Serial.println(fir&0x000F);
	  Serial.println((fir&0x00F0)>>4);
	  Serial.println((fir&0x0F00)>>8);
	  Serial.println((fir&0xF000)>>12);

	  //if (fir == 0x4D42)
	  //{
		Serial.println("length width : ");
		read32(bmpFS);
		read32(bmpFS);
		seekOffset = read32(bmpFS);
		read32(bmpFS);
		w = read32(bmpFS);
		h = read32(bmpFS);
		Serial.println(w);
		Serial.println(h);

		if ((read16(bmpFS) == 1) && (read16(bmpFS) == 24) && (read32(bmpFS) == 0))
		{
		  y += h - 1;

		  bool oldSwapBytes = tft.getSwapBytes();
		  tft.setSwapBytes(true);
		  bmpFS.seek(seekOffset);

		  uint16_t padding = (4 - ((w * 3) & 3)) & 3;
		  uint8_t lineBuffer[w * 3 + padding];

		  for (row = 0; row < h; row++) {
			
			bmpFS.read(lineBuffer, sizeof(lineBuffer));
			uint8_t*  bptr = lineBuffer;
			uint16_t* tptr = (uint16_t*)lineBuffer;
			// Convert 24 to 16 bit colours
			for (uint16_t col = 0; col < w; col++)
			{
			  b = *bptr++;
			  g = *bptr++;
			  r = *bptr++;
			  *tptr++ = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
			}

			// Push the pixel row to screen, pushImage will crop the line if needed
			// y is decremented as the BMP image is drawn bottom up
			tft.pushImage(x, y--, w, 1, (uint16_t*)lineBuffer);
		  }
		  tft.setSwapBytes(oldSwapBytes);
		  Serial.print("Loaded in "); Serial.print(millis() - startTime);
		  Serial.println(" ms");
		//}
		//else Serial.println("BMP format not recognized.");
	  }
	  bmpFS.close();
	}

	// These read 16- and 32-bit types from the SD card file.
	// BMP data is stored little-endian, Arduino is little-endian too.
	// May need to reverse subscript order if porting elsewhere.

	uint16_t read16(fs::File &f) {
	  uint16_t result;
	  ((uint8_t *)&result)[0] = f.read(); // LSB
	  ((uint8_t *)&result)[1] = f.read(); // MSB
	  return result;
	}

	uint32_t read32(fs::File &f) {
	  uint32_t result;
	  ((uint8_t *)&result)[0] = f.read(); // LSB
	  ((uint8_t *)&result)[1] = f.read();
	  ((uint8_t *)&result)[2] = f.read();
	  ((uint8_t *)&result)[3] = f.read(); // MSB
	  return result;
	}
};

#endif