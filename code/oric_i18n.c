#include "oric_i18n.h"
#include <string.h>

static const wchar_t *const TRANSLATIONS[STR_ID_COUNT][ORIC_LANG_COUNT] = {
    /* STR_APP_TITLE */
    {
        L"Oric Explorer — TAP, DSK (SEDORIC/FT-DOS/OricDOS) & ROM Inspector",
        L"Oric Explorer — K7, Disquettes (SEDORIC/FT-DOS/OricDOS) & ROMs",
        L"Oric Explorer — Kassetten, Disketten & ROMs",
        L"Oric Explorer — Cintas, Discos y ROMs Oric"
    },
    /* STR_APP_TITLE_FILE_FMT */
    {
        L"%ls — Oric Explorer",
        L"%ls — Oric Explorer",
        L"%ls — Oric Explorer",
        L"%ls — Oric Explorer"
    },

    /* Menus */
    { L"&File",      L"&Fichier",   L"&Datei",      L"&Archivo" },
    { L"&Open...\tCtrl+O", L"&Ouvrir...\tCtrl+O", L"&Öffnen...\tCtrl+O", L"&Abrir...\tCtrl+O" },
    { L"Recent &Files",    L"Fichiers &récents",  L"&Zuletzt geöffnet",   L"Archivos &recientes" },
    { L"(Empty)",          L"(Vide)",             L"(Leer)",              L"(Vacío)" },
    { L"&Back to Explorer\tEsc", L"&Retour à l'explorateur\tÉchap", L"&Zurück zum Explorer\tEsc", L"&Volver al explorador\tEsc" },
    { L"E&xit\tAlt+F4",    L"&Quitter\tAlt+F4",   L"&Beenden\tAlt+F4",    L"&Salir\tAlt+F4" },
    { L"&Edit",            L"&Édition",           L"&Bearbeiten",         L"&Edición" },
    { L"&Copy Active View\tCtrl+C", L"&Copier la vue active\tCtrl+C", L"&Aktive Ansicht kopieren\tCtrl+C", L"&Copiar vista activa\tCtrl+C" },
    { L"&Tools",           L"&Outils",            L"&Werkzeuge",          L"&Herramientas" },
    { L"Extract Raw &Binary (.bin)...", L"Extraire le &binaire brut (.bin)...", L"Rohdaten &extrahieren (.bin)...", L"Extraer &binario (.bin)..." },
    { L"Export Selection as &Tape (.tap)...", L"Exporter en &cassette (.tap)...", L"Als &Kassette exportieren (.tap)...", L"Exportar como &cinta (.tap)..." },
    { L"Export Selection as &Audio (.wav)...", L"Exporter en &audio (.wav)...", L"Als &Audio exportieren (.wav)...", L"Exportar como &audio (.wav)..." },
    { L"Save Image as &PNG (.png)...", L"Enregistrer l'image en &PNG (.png)...", L"Bild als &PNG speichern (.png)...", L"Guardar imagen como &PNG (.png)..." },
    { L"&Launch in Emulator\tF5", L"&Lancer dans l'émulateur\tF5", L"Im &Emulator starten\tF5", L"&Abrir en emulador\tF5" },
    { L"Configure &Emulators (.tap / .dsk)...", L"Configurer les &émulateurs (.tap / .dsk)...", L"&Emulatoren konfigurieren (.tap / .dsk)...", L"Configurar &emuladores (.tap / .dsk)..." },
    { L"&Language",  L"&Langue",    L"&Sprache",    L"&Idioma" },
    { L"English",    L"English",    L"English",     L"English" },
    { L"Français",   L"Français",   L"Français",    L"Français" },
    { L"Deutsch",    L"Deutsch",    L"Deutsch",     L"Deutsch" },
    { L"Español",    L"Español",    L"Español",     L"Español" },
    { L"&Help",      L"&Aide",      L"&Hilfe",      L"&Ayuda" },
    { L"&About Oric Explorer...", L"À &propos d'Oric Explorer...", L"&Über Oric Explorer...", L"&Acerca de Oric Explorer..." },

    /* Context menu */
    { L"View &Code / BASIC / 6502", L"Voir &Code / BASIC / 6502", L"&Code / BASIC / 6502 ansehen", L"Ver &Código / BASIC / 6502" },
    { L"View &Screen / Sector Map", L"Voir &Écran / Carte secteurs", L"&Bildschirm / Sektorkarte", L"Ver &Pantalla / Mapa de sectores" },

    /* File Explorer */
    { L"< Explorer (Esc)", L"< Explorateur (Échap)", L"< Explorer (Esc)", L"< Explorador (Esc)" },
    { L"Up",               L"Parent",                 L"Hoch",             L"Subir" },
    { L"Path:",            L"Dossier :",              L"Pfad:",            L"Ruta:" },
    { L"Drive:",           L"Lecteur :",              L"Laufwerk:",        L"Unidad:" },
    { L"Name",             L"Nom",                    L"Name",             L"Nombre" },
    { L"Type",             L"Type",                   L"Typ",              L"Tipo" },
    { L"Size",             L"Taille",                 L"Größe",            L"Tamaño" },
    { L"Modified",         L"Modifié",                L"Geändert",         L"Modificado" },
    { L"Folder",           L"Dossier",                L"Ordner",           L"Carpeta" },
    { L"Oric Tape (.tap)", L"Cassette (.tap)",        L"Oric-Kassette (.tap)", L"Cinta Oric (.tap)" },
    { L"Oric Disk (.dsk)", L"Disquette (.dsk)",       L"Oric-Diskette (.dsk)", L"Disco Oric (.dsk)" },
    { L"ROM Image",        L"Image ROM",              L"ROM-Image",        L"Imagen ROM" },
    {
        L"Explorer: %zu item(s) in %ls",
        L"Explorateur : %zu élément(s) dans %ls",
        L"Explorer: %zu Element(e) in %ls",
        L"Explorador: %zu elemento(s) en %ls"
    },
    {
        L"FILE EXPLORER\r\n"
        L"========================================\r\n"
        L"Folder : %ls\r\n\r\n"
        L"Double-click or press Enter on any\r\n"
        L".tap, .dsk, .rom or .bin file to inspect\r\n"
        L"its contents.\r\n\r\n"
        L"Shortcuts:\r\n"
        L"  • Enter : Open folder / inspect media\r\n"
        L"  • Backspace : Parent folder\r\n"
        L"  • F2 : Rename file / folder\r\n"
        L"  • Del : Move to Recycle Bin\r\n"
        L"  • Esc : Exit analyzer -> Explorer\r\n"
        L"  • F5 : Launch media in emulator\r\n",

        L"EXPLORATEUR DE FICHIERS\r\n"
        L"========================================\r\n"
        L"Dossier : %ls\r\n\r\n"
        L"Double-cliquez ou appuyez sur Entrée\r\n"
        L"sur un fichier .tap, .dsk, .rom ou .bin\r\n"
        L"pour l'analyser.\r\n\r\n"
        L"Raccourcis :\r\n"
        L"  • Entrée : Ouvrir dossier / analyser\r\n"
        L"  • Retour arrière : Dossier parent\r\n"
        L"  • F2 : Renommer fichier / dossier\r\n"
        L"  • Suppr : Mettre à la corbeille\r\n"
        L"  • Échap : Quitter l'analyseur -> Explorateur\r\n"
        L"  • F5 : Lancer dans l'émulateur\r\n",

        L"DATEI-EXPLORER\r\n"
        L"========================================\r\n"
        L"Ordner : %ls\r\n\r\n"
        L"Doppelklicken Sie oder drücken Sie Enter\r\n"
        L"auf einer .tap-, .dsk- oder .rom-Datei,\r\n"
        L"um sie zu analysieren.\r\n\r\n"
        L"Tastenkürzel:\r\n"
        L"  • Enter : Ordner öffnen / analysieren\r\n"
        L"  • Rücktaste : Übergeordneter Ordner\r\n"
        L"  • F2 : Datei / Ordner umbenennen\r\n"
        L"  • Entf : In den Papierkorb verschieben\r\n"
        L"  • Esc : Zurück zum Explorer\r\n"
        L"  • F5 : Im Emulator starten\r\n",

        L"EXPLORADOR DE ARCHIVOS\r\n"
        L"========================================\r\n"
        L"Carpeta : %ls\r\n\r\n"
        L"Haga doble clic o pulse Enter sobre\r\n"
        L"un archivo .tap, .dsk, .rom o .bin\r\n"
        L"para analizarlo.\r\n\r\n"
        L"Atajos:\r\n"
        L"  • Enter : Abrir carpeta / analizar\r\n"
        L"  • Retroceso : Carpeta superior\r\n"
        L"  • F2 : Renombrar archivo / carpeta\r\n"
        L"  • Supr : Enviar a la papelera\r\n"
        L"  • Esc : Salir del analizador -> Explorador\r\n"
        L"  • F5 : Abrir en emulador\r\n"
    },
    /* STR_EXP_PREVIEW_CONTENTS_FMT */
    {
        L"\r\nMEDIA CONTENTS (%zu)\r\n----------------------------------------\r\n",
        L"\r\nCONTENU DU MÉDIA (%zu)\r\n----------------------------------------\r\n",
        L"\r\nMEDIUMINHALT (%zu)\r\n----------------------------------------\r\n",
        L"\r\nCONTENIDO DEL MEDIO (%zu)\r\n----------------------------------------\r\n"
    },
    /* STR_EXP_CTX_OPEN */
    {
        L"&Open / Inspect\tEnter",
        L"&Ouvrir / Analyser\tEntrée",
        L"&Öffnen / Analysieren\tEnter",
        L"&Abrir / Analizar\tEnter"
    },
    /* STR_EXP_CTX_RENAME */
    {
        L"&Rename...\tF2",
        L"&Renommer...\tF2",
        L"&Umbenennen...\tF2",
        L"&Renombrar...\tF2"
    },
    /* STR_EXP_CTX_DELETE */
    {
        L"Move to &Recycle Bin\tDel",
        L"Mettre à la &corbeille\tSuppr",
        L"In den &Papierkorb\tEntf",
        L"Enviar a la &papelera\tSupr"
    },
    /* STR_RENAME_DLG_TITLE */
    { L"Rename", L"Renommer", L"Umbenennen", L"Renombrar" },
    /* STR_RENAME_LABEL */
    { L"New name:", L"Nouveau nom :", L"Neuer Name:", L"Nuevo nombre:" },
    /* STR_ERR_RENAME_TITLE */
    { L"Rename", L"Renommer", L"Umbenennen", L"Renombrar" },
    /* STR_ERR_RENAME_EMPTY */
    {
        L"The name cannot be empty.",
        L"Le nom ne peut pas être vide.",
        L"Der Name darf nicht leer sein.",
        L"El nombre no puede estar vacío."
    },
    /* STR_ERR_RENAME_CHARS */
    {
        L"A file name cannot contain any of the following characters:\r\n\\ / : * ? \" < > |",
        L"Un nom de fichier ne peut pas contenir les caractères suivants :\r\n\\ / : * ? \" < > |",
        L"Ein Dateiname darf keines der folgenden Zeichen enthalten:\r\n\\ / : * ? \" < > |",
        L"Un nombre de archivo no puede contener ninguno de estos caracteres:\r\n\\ / : * ? \" < > |"
    },
    /* STR_ERR_RENAME_FAIL */
    {
        L"Unable to rename the selected item (it may already exist or be in use).",
        L"Impossible de renommer l'élément sélectionné (il existe peut-être déjà ou est utilisé).",
        L"Das ausgewählte Element konnte nicht umbenannt werden (evtl. existiert es bereits oder wird verwendet).",
        L"No se pudo renombrar el elemento seleccionado (puede que ya exista o esté en uso)."
    },
    /* STR_ERR_DELETE_TITLE */
    {
        L"Move to Recycle Bin",
        L"Mettre à la corbeille",
        L"In den Papierkorb",
        L"Enviar a la papelera"
    },
    /* STR_ERR_DELETE_FAIL */
    {
        L"Unable to move the selected item to the Recycle Bin.",
        L"Impossible d'envoyer l'élément sélectionné à la corbeille.",
        L"Das ausgewählte Element konnte nicht in den Papierkorb verschoben werden.",
        L"No se pudo enviar el elemento seleccionado a la papelera."
    },
    /* STR_STATUS_RENAMED_FMT */
    {
        L"Renamed: %ls",
        L"Renommé : %ls",
        L"Umbenannt: %ls",
        L"Renombrado: %ls"
    },
    /* STR_STATUS_DELETED_FMT */
    {
        L"Moved to Recycle Bin: %ls",
        L"Mis à la corbeille : %ls",
        L"In den Papierkorb verschoben: %ls",
        L"Enviado a la papelera: %ls"
    },

    /* Columns & Filter */
    { L"Filter:",             L"Filtre :",            L"Filter:",              L"Filtro:" },
    { L"Name",                L"Nom",                 L"Name",                 L"Nombre" },
    { L"Type",                L"Type",                L"Typ",                  L"Tipo" },
    { L"Size",                L"Taille",              L"Größe",                L"Tamaño" },
    { L"Addresses / Range",   L"Adresses / Plage",    L"Adressen / Bereich",   L"Direcciones / Rango" },
    { L"Attributes / Details",L"Attributs / Détails", L"Attribute / Details",  L"Atributos / Detalles" },
    { L"CRC32",               L"CRC32",               L"CRC32",                L"CRC32" },

    /* Sidebar, Tabs & StatusBar */
    {
        L"Inspector & Previews",
        L"Inspecteur & Aperçus",
        L"Inspektor & Vorschau",
        L"Inspector y Vistas"
    },
    { L"Summary",      L"Résumé",       L"Übersicht",  L"Resumen" },
    { L"Code / Hex",   L"Code / Hexa",  L"Code / Hex", L"Código / Hex" },
    { L"Screen / Map", L"Écran / Carte",L"Bild / Karte",L"Pantalla / Mapa" },
    {
        L"Copy Text (Ctrl+C)",
        L"Copier le texte (Ctrl+C)",
        L"Text kopieren (Ctrl+C)",
        L"Copiar texto (Ctrl+C)"
    },
    { L"Save Image as PNG...", L"Enregistrer PNG...", L"Als PNG speichern...", L"Guardar PNG..." },
    { L"Show Hex Dump",    L"Voir Hexa / ASCII",   L"Hex-Dump zeigen",   L"Ver Hex / ASCII" },
    { L"Show BASIC / 6502",L"Voir BASIC / 6502",   L"BASIC / 6502 zeigen",L"Ver BASIC / 6502" },
    {
        L"Explorer Ready — Double-click a .tap, .dsk or .rom file",
        L"Explorateur prêt — Double-cliquez sur un fichier .tap, .dsk ou .rom",
        L"Explorer bereit — Doppelklicken Sie auf eine .tap-, .dsk- oder .rom-Datei",
        L"Explorador listo — Haga doble clic en un archivo .tap, .dsk o .rom"
    },
    { L"Explorer",   L"Explorateur", L"Explorer",    L"Explorador" },
    { L"0 items",    L"0 élément",   L"0 Elemente",  L"0 elementos" },
    {
        L"Text copied to clipboard.",
        L"Texte copié dans le presse-papier.",
        L"Text in die Zwischenablage kopiert.",
        L"Texto copiado al portapapeles."
    },
    {
        L"Exported successfully: %ls (%zu bytes)",
        L"Export réussi : %ls (%zu octets)",
        L"Erfolgreich exportiert: %ls (%zu Bytes)",
        L"Exportado correctamente: %ls (%zu bytes)"
    },
    {
        L"Launched emulator with current media.",
        L"Émulateur lancé avec le média actuel.",
        L"Emulator mit aktuellem Medium gestartet.",
        L"Emulador iniciado con el medio actual."
    },
    {
        L"Emulator settings saved to OricExplorer.ini.",
        L"Réglages émulateurs enregistrés dans OricExplorer.ini.",
        L"Emulator-Einstellungen in OricExplorer.ini gespeichert.",
        L"Ajustes del emulador guardados en OricExplorer.ini."
    },
    { L"Unknown",    L"Inconnu",     L"Unbekannt",   L"Desconocido" },
    { L"Tape .TAP",  L"Cassette .TAP", L"Kassette .TAP", L"Cinta .TAP" },
    { L"DSK (SEDORIC)", L"DSK (SEDORIC)", L"DSK (SEDORIC)", L"DSK (SEDORIC)" },
    { L"DSK (MFM_DISK / SEDORIC)", L"DSK (MFM_DISK / SEDORIC)", L"DSK (MFM_DISK / SEDORIC)", L"DSK (MFM_DISK / SEDORIC)" },
    { L"DSK (FT-DOS)",  L"DSK (FT-DOS)",  L"DSK (FT-DOS)",  L"DSK (FT-DOS)" },
    { L"DSK (OricDOS)", L"DSK (OricDOS)", L"DSK (OricDOS)", L"DSK (OricDOS)" },
    { L"DSK (StratSED)",L"DSK (StratSED)",L"DSK (StratSED)",L"DSK (StratSED)" },
    { L"Original Oric-1 ROM",      L"ROM Oric-1 d'origine",     L"Original Oric-1-ROM",      L"ROM Oric-1 original" },
    { L"Original Atmos ROM",       L"ROM Atmos d'origine",      L"Original Atmos-ROM",       L"ROM Atmos original" },
    { L"Original Telestrat ROM",   L"ROM Telestrat d'origine",  L"Original Telestrat-ROM",   L"ROM Telestrat original" },
    { L"Official Oric ROM",        L"ROM Oric officielle",      L"Offizielles Oric-ROM",     L"ROM Oric oficial" },
    { L"ROM / Binary",             L"ROM / Binaire",            L"ROM / Binärdatei",         L"ROM / Binario" },
    {
        L"%u bytes | CRC 0x%08X",
        L"%u octets | CRC 0x%08X",
        L"%u Bytes | CRC 0x%08X",
        L"%u bytes | CRC 0x%08X"
    },
    {
        L"%zu item(s) | %u bytes",
        L"%zu élément(s) | %u octets",
        L"%zu Element(e) | %u Bytes",
        L"%zu elemento(s) | %u bytes"
    },
    { L"bytes", L"octets", L"Bytes", L"bytes" },
    { L"KB",    L"Ko",     L"KB",    L"KB" },
    { L"%u bytes", L"%u octets", L"%u Bytes", L"%u bytes" },

    /* Emulator Configuration Dialog */
    {
        L"Configure Emulators (OricExplorer.ini)",
        L"Configurer les émulateurs (OricExplorer.ini)",
        L"Emulatoren konfigurieren (OricExplorer.ini)",
        L"Configurar emuladores (OricExplorer.ini)"
    },
    {
        L"Emulator for .TAP (Cassettes):",
        L"Émulateur pour .TAP (Cassettes) :",
        L"Emulator für .TAP (Kassetten):",
        L"Emulador para .TAP (Cintas):"
    },
    {
        L"Emulator for .DSK (Floppy Disks):",
        L"Émulateur pour .DSK (Disquettes) :",
        L"Emulator für .DSK (Disketten):",
        L"Emulador para .DSK (Discos):"
    },
    {
        L"Args (%s = file):",
        L"Arguments (%s = fichier) :",
        L"Parameter (%s = Datei):",
        L"Parámetros (%s = archivo):"
    },
    { L"Browse...", L"Parcourir...", L"Durchsuchen...", L"Examinar..." },
    { L"Save",      L"Enregistrer",  L"Speichern",      L"Guardar" },
    { L"Cancel",    L"Annuler",      L"Abbrechen",      L"Cancelar" },
    {
        L"Tip: Use %s in Arguments for the media path (e.g. \"%s\" or --disk \"%s\"). Saved to OricExplorer.ini.",
        L"Astuce : Utilisez %s dans les arguments pour le chemin du fichier (ex : \"%s\" ou --disk \"%s\"). Enregistré dans OricExplorer.ini.",
        L"Tipp: Verwenden Sie %s als Platzhalter für den Dateipfad (z. B. \"%s\" oder --disk \"%s\"). Gespeichert in OricExplorer.ini.",
        L"Consejo: Use %s en los parámetros para la ruta del archivo (ej. \"%s\" o --disk \"%s\"). Guardado en OricExplorer.ini."
    },

    /* Dialogs & Errors */
    { L"Read Error", L"Erreur de lecture", L"Lesefehler", L"Error de lectura" },
    {
        L"Unable to open the selected file.",
        L"Impossible d'ouvrir le fichier sélectionné.",
        L"Die ausgewählte Datei konnte nicht geöffnet werden.",
        L"No se pudo abrir el archivo seleccionado."
    },
    {
        L"Invalid file size or greater than 16 MB.",
        L"Taille de fichier invalide ou supérieure à 16 Mo.",
        L"Ungültige Dateigröße oder größer als 16 MB.",
        L"Tamaño de archivo no válido o superior a 16 MB."
    },
    { L"Error", L"Erreur", L"Fehler", L"Error" },
    {
        L"Insufficient memory.",
        L"Mémoire insuffisante.",
        L"Nicht genügend Arbeitsspeicher.",
        L"Memoria insuficiente."
    },
    {
        L"Error reading file contents.",
        L"Erreur lors de la lecture du contenu du fichier.",
        L"Fehler beim Lesen des Dateiinhalts.",
        L"Error al leer el contenido del archivo."
    },
    {
        L"Unable to write the destination file.",
        L"Impossible d'écrire le fichier de destination.",
        L"Die Zieldatei konnte nicht geschrieben werden.",
        L"No se pudo escribir el archivo de destino."
    },
    {
        L"Select a program or file with extractable data first.",
        L"Sélectionnez d'abord un programme ou fichier contenant des données extractibles.",
        L"Wählen Sie zuerst ein Programm oder eine Datei mit extrahierbaren Daten.",
        L"Seleccione primero un programa o archivo con datos extraíbles."
    },
    {
        L"No HIRES/TEXT image or disk sector map is currently rendered.",
        L"Aucune image HIRES/TEXT ou carte disquette n'est affichée actuellement.",
        L"Derzeit wird kein HIRES/TEXT-Bild und keine Sektorkarte angezeigt.",
        L"No hay ninguna imagen HIRES/TEXT ni mapa de disco visible actualmente."
    },
    { L"Emulator", L"Émulateur", L"Emulator", L"Emulador" },
    {
        L"No emulator executable is configured for this media type. Please configure it now.",
        L"Aucun émulateur n'est configuré pour ce type de média. Veuillez le configurer.",
        L"Für diesen Medientyp ist kein Emulator konfiguriert. Bitte konfigurieren.",
        L"No hay ningún emulador configurado para este tipo de medio. Configúrelo ahora."
    },
    {
        L"Failed to launch the configured emulator with the current media file.",
        L"Échec du lancement de l'émulateur configuré avec le média actuel.",
        L"Der konfigurierte Emulator konnte nicht mit dem aktuellen Medium gestartet werden.",
        L"Error al iniciar el emulador configurado con el archivo actual."
    },
    {
        L"Unable to register the window class.",
        L"Impossible d'enregistrer la classe de fenêtre.",
        L"Fensterklasse konnte nicht registriert werden.",
        L"No se pudo registrar la clase de ventana."
    },
    {
        L"Unable to create the main window.",
        L"Impossible de créer la fenêtre principale.",
        L"Hauptfenster konnte nicht erstellt werden.",
        L"No se pudo crear la ventana principal."
    },
    { L"Fatal Error", L"Erreur fatale", L"Schwerwiegender Fehler", L"Error fatal" },
    {
        L"Open an Oric tape, floppy disk, or ROM",
        L"Ouvrir une K7, disquette ou ROM Oric",
        L"Oric-Kassette, -Diskette oder -ROM öffnen",
        L"Abrir una cinta, disco o ROM de Oric"
    },
    {
        L"Extract raw binary (.bin)",
        L"Extraire le binaire brut (.bin)",
        L"Rohdaten speichern (.bin)",
        L"Extraer binario (.bin)"
    },
    {
        L"Export as Oric tape (.tap)",
        L"Exporter en cassette Oric (.tap)",
        L"Als Oric-Kassette speichern (.tap)",
        L"Exportar como cinta Oric (.tap)"
    },
    {
        L"Export as Oric tape audio (.wav)",
        L"Exporter en audio cassette Oric (.wav)",
        L"Als Kassetten-Audio speichern (.wav)",
        L"Exportar como audio de cinta (.wav)"
    },
    {
        L"Save extracted image as PNG (.png)",
        L"Enregistrer l'image extraite en PNG (.png)",
        L"Extrahiertes Bild als PNG speichern (.png)",
        L"Guardar imagen extraída como PNG (.png)"
    },
    {
        L"Select emulator executable (.exe)",
        L"Sélectionner l'exécutable de l'émulateur (.exe)",
        L"Emulator-Programm auswählen (.exe)",
        L"Seleccionar ejecutable del emulador (.exe)"
    },
    {
        L"Oric Files (*.tap;*.dsk;*.rom;*.bin)",
        L"Fichiers Oric (*.tap;*.dsk;*.rom;*.bin)",
        L"Oric-Dateien (*.tap;*.dsk;*.rom;*.bin)",
        L"Archivos Oric (*.tap;*.dsk;*.rom;*.bin)"
    },
    { L"Oric Tapes (*.tap)",        L"Cassettes Oric (*.tap)",   L"Oric-Kassetten (*.tap)",  L"Cintas Oric (*.tap)" },
    { L"Oric Floppy Disks (*.dsk)", L"Disquettes Oric (*.dsk)",  L"Oric-Disketten (*.dsk)",  L"Discos Oric (*.dsk)" },
    { L"ROM Images (*.rom;*.bin)",  L"Images ROM (*.rom;*.bin)", L"ROM-Images (*.rom;*.bin)",L"Imágenes ROM (*.rom;*.bin)" },
    { L"Binary Files (*.bin)",      L"Fichiers binaires (*.bin)",L"Binärdateien (*.bin)",    L"Archivos binarios (*.bin)" },
    { L"WAVE Audio (*.wav)",        L"Audio WAVE (*.wav)",       L"WAVE-Audio (*.wav)",      L"Audio WAVE (*.wav)" },
    { L"PNG Images (*.png)",        L"Images PNG (*.png)",       L"PNG-Bilder (*.png)",      L"Imágenes PNG (*.png)" },
    { L"Executables (*.exe)",       L"Exécutables (*.exe)",      L"Programme (*.exe)",       L"Ejecutables (*.exe)" },
    { L"All Files (*.*)",           L"Tous les fichiers (*.*)",  L"Alle Dateien (*.*)",      L"Todos los archivos (*.*)" },
    {
        L"About Oric Explorer",
        L"À propos d'Oric Explorer",
        L"Über Oric Explorer",
        L"Acerca de Oric Explorer"
    },
    {
        L"Oric Explorer v2.1\r\n"
        L"Native Win32 Unicode C application (zero external dependencies).\r\n\r\n"
        L"Features:\r\n"
        L"  • Built-in File Explorer for .tap, .dsk & .rom (Esc to return)\r\n"
        L"  • .tap cassettes, .dsk disks (SEDORIC, StratSED, FT-DOS, OricDOS)\r\n"
        L"  • Oric-1, Atmos, Telestrat, Pravetz 8D & Disk ROM verification\r\n"
        L"  • Built-in BASIC detokenizer, 6502 disassembler & Hex viewer\r\n"
        L"  • Native HIRES/TEXT screen viewer + PNG export & sector map\r\n"
        L"  • Portable OricExplorer.ini settings & separate .tap/.dsk emulators (F5)",

        L"Oric Explorer v2.1\r\n"
        L"Application Win32 native en C Unicode (sans dépendance externe).\r\n\r\n"
        L"Fonctionnalités :\r\n"
        L"  • Explorateur de fichiers intégré (.tap, .dsk, .rom — Échap pour revenir)\r\n"
        L"  • Cassettes .tap, disquettes .dsk (SEDORIC, StratSED, FT-DOS, OricDOS)\r\n"
        L"  • Vérification ROM Oric-1, Atmos, Telestrat, Pravetz 8D & contrôleurs\r\n"
        L"  • Décodeur BASIC intégré, désassembleur 6502 & vue Hexa/ASCII\r\n"
        L"  • Visionneuse HIRES/TEXT + export PNG & carte des secteurs disquette\r\n"
        L"  • Réglages portables dans OricExplorer.ini & émulateurs .tap/.dsk (F5)",

        L"Oric Explorer v2.1\r\n"
        L"Native Win32-Unicode-C-Anwendung (ohne externe Abhängigkeiten).\r\n\r\n"
        L"Funktionen:\r\n"
        L"  • Integrierter Datei-Explorer für .tap, .dsk & .rom (Esc zum Zurückkehren)\r\n"
        L"  • .tap-Kassetten, .dsk-Disketten (SEDORIC, StratSED, FT-DOS, OricDOS)\r\n"
        L"  • Oric-1, Atmos, Telestrat, Pravetz 8D & Controller-ROM-Prüfung\r\n"
        L"  • Integrierter BASIC-Detokenizer, 6502-Disassembler & Hex-Ansicht\r\n"
        L"  • HIRES/TEXT-Vorschau + PNG-Export & Disk-Sektorkarte\r\n"
        L"  • Portable OricExplorer.ini & konfigurierbare .tap/.dsk-Emulatoren (F5)",

        L"Oric Explorer v2.1\r\n"
        L"Aplicación nativa Win32 en C Unicode (sin dependencias externas).\r\n\r\n"
        L"Funciones:\r\n"
        L"  • Explorador integrado para .tap, .dsk y .rom (Esc para volver)\r\n"
        L"  • Cintas .tap, discos .dsk (SEDORIC, StratSED, FT-DOS, OricDOS)\r\n"
        L"  • Verificación de ROMs Oric-1, Atmos, Telestrat, Pravetz 8D y discos\r\n"
        L"  • Decodificador BASIC, desensamblador 6502 y visor Hex/ASCII\r\n"
        L"  • Visor HIRES/TEXT + exportación PNG y mapa de sectores\r\n"
        L"  • Ajustes en OricExplorer.ini y emuladores configurables .tap/.dsk (F5)"
    },

    /* General detection */
    {
        L"Empty or unreadable file.",
        L"Fichier vide ou illisible.",
        L"Leere oder unlesbare Datei.",
        L"Archivo vacío o ilegible."
    },
    {
        L"Unrecognized format. Supported formats: .tap (tape), .dsk (disk), .rom/.bin (ROM).",
        L"Format non reconnu. Formats supportés : .tap (K7), .dsk (disquette), .rom/.bin (ROM).",
        L"Unbekanntes Format. Unterstützt: .tap (Kassette), .dsk (Diskette), .rom/.bin (ROM).",
        L"Formato no reconocido. Formatos compatibles: .tap (cinta), .dsk (disco), .rom/.bin (ROM)."
    },

    /* TAP parser */
    {
        L"Invalid or truncated .tap file (%zu bytes).",
        L"Fichier .tap trop court ou invalide (%zu octets).",
        L"Ungültige oder zu kurze .tap-Datei (%zu Bytes).",
        L"Archivo .tap demasiado corto o no válido (%zu bytes)."
    },
    { L"<Unnamed #%d>", L"<Sans nom #%d>", L"<Unbenannt #%d>", L"<Sin nombre #%d>" },
    { L"Screen / Memory", L"Écran / Mémoire", L"Bildschirm / Speicher", L"Pantalla / Memoria" },
    { L"Binary (ML)", L"Binaire (LM)", L"Binär (Maschinencode)", L"Binario (CM)" },
    { L"Array", L"Tableau", L"Feld (Array)", L"Matriz" },
    { L"Type 0x%02X", L"Type 0x%02X", L"Typ 0x%02X", L"Tipo 0x%02X" },
    {
        L"Autorun (0x%02X), Truncated",
        L"Autorun (0x%02X), Tronqué",
        L"Autostart (0x%02X), Abgeschnitten",
        L"Autoarranque (0x%02X), Truncado"
    },
    {
        L"Autorun (0x%02X)",
        L"Autorun (0x%02X)",
        L"Autostart (0x%02X)",
        L"Autoarranque (0x%02X)"
    },
    { L"Truncated data", L"Données tronquées", L"Abgeschnittene Daten", L"Datos truncados" },
    { L"Standard", L"Standard", L"Standard", L"Estándar" },
    {
        L"Block #%d | Header type=0x%02X, autorun=0x%02X, %u bytes",
        L"Bloc #%d | En-tête type=0x%02X, autorun=0x%02X, %u octets",
        L"Block #%d | Header Typ=0x%02X, Autostart=0x%02X, %u Bytes",
        L"Bloque #%d | Cabecera tipo=0x%02X, autoarranque=0x%02X, %u bytes"
    },
    {
        L"No valid Oric tape block (0x16 / 0x24 sync) detected.",
        L"Aucun bloc K7 Oric valide (synchro 0x16 / 0x24) détecté.",
        L"Kein gültiger Oric-Kassettenblock (0x16 / 0x24 Sync) gefunden.",
        L"No se detectó ningún bloque de cinta Oric válido (sinc. 0x16 / 0x24)."
    },
    {
        L"Tape .tap analyzed: %zu program(s) found.",
        L"Cassette .tap analysée : %zu programme(s) trouvé(s).",
        L".tap-Kassette analysiert: %zu Programm(e) gefunden.",
        L"Cinta .tap analizada: %zu programa(s) encontrado(s)."
    },

    /* DSK parser */
    {
        L"Invalid or too short .dsk file.",
        L"Fichier .dsk trop court ou invalide.",
        L"Ungültige oder zu kurze .dsk-Datei.",
        L"Archivo .dsk demasiado corto o no válido."
    },
    {
        L"Unrecognized .dsk signature (expected ORICDISK or MFM_DISK).",
        L"Signature .dsk non reconnue (attendu ORICDISK ou MFM_DISK).",
        L"Unbekannte .dsk-Signatur (ORICDISK oder MFM_DISK erwartet).",
        L"Firma .dsk no reconocida (se esperaba ORICDISK o MFM_DISK)."
    },
    { L"(Unnamed)", L"(Sans nom)", L"(Unbenannt)", L"(Sin nombre)" },
    {
        L"Unknown / custom disk format",
        L"Format disque inconnu / propriétaire",
        L"Unbekanntes / eigenes Disk-Format",
        L"Formato de disco desconocido / propio"
    },
    {
        L"Disk image too small (track 20 missing).",
        L"Image .dsk trop petite (piste 20 absente).",
        L"Disk-Image zu klein (Spur 20 fehlt).",
        L"Imagen de disco demasiado pequeña (falta la pista 20)."
    },
    { L"HIRES Screen", L"Écran HIRES", L"HIRES-Bildschirm", L"Pantalla HIRES" },
    { L"Text",         L"Texte",       L"Text",             L"Texto" },
    { L"Data",         L"Données",     L"Daten",            L"Datos" },
    { L"System",       L"Système",     L"System",           L"Sistema" },
    { L"DOS File",     L"Fichier DOS", L"DOS-Datei",        L"Archivo DOS" },
    {
        L"Protected (%u sect.)",
        L"Protégé (%u sect.)",
        L"Geschützt (%u Sekt.)",
        L"Protegido (%u sect.)"
    },
    {
        L"%u sect.",
        L"%u sect.",
        L"%u Sekt.",
        L"%u sect."
    },
    {
        L"Descriptor S%u T%02u Sec%02u | Attr 0x%02X",
        L"Descripteur F%u P%02u S%02u | Attr 0x%02X",
        L"Deskriptor S%u Sp%02u Sek%02u | Attr 0x%02X",
        L"Descriptor C%u P%02u S%02u | Attr 0x%02X"
    },
    {
        L"Start S%u T%02u Sec%02u | Type 0x%02X",
        L"Début F%u P%02u S%02u | Type 0x%02X",
        L"Start S%u Sp%02u Sek%02u | Typ 0x%02X",
        L"Inicio C%u P%02u S%02u | Tipo 0x%02X"
    },
    {
        L"Disk %ls (%ls): %zu file(s) listed.",
        L"Disquette %ls (%ls) : %zu fichier(s) listé(s).",
        L"Diskette %ls (%ls): %zu Datei(en) aufgelistet.",
        L"Disco %ls (%ls): %zu archivo(s) listado(s)."
    },
    {
        L"Empty %ls (%ls) disk (0 files).",
        L"Disquette %ls (%ls) vide (0 fichier).",
        L"Leere %ls-Diskette (%ls, 0 Dateien).",
        L"Disco %ls (%ls) vacío (0 archivos)."
    },
    {
        L"%ls disk loaded, no supported DOS catalog found.",
        L"Disquette %ls chargée, aucun catalogue DOS reconnu.",
        L"%ls-Diskette geladen, kein unterstützter DOS-Katalog gefunden.",
        L"Disco %ls cargado, no se encontró catálogo DOS compatible."
    },

    /* ROM analyzer */
    { L"Oric Atmos (Variant)", L"Oric Atmos (Variante)", L"Oric Atmos (Variante)", L"Oric Atmos (Variante)" },
    { L"Microdisc Controller", L"Contrôleur Microdisc",  L"Microdisc-Controller",  L"Controladora Microdisc" },
    { L"Microdisc EPROM",      L"EPROM Microdisc",       L"Microdisc-EPROM",       L"EPROM Microdisc" },
    { L"Jasmin Controller",    L"Contrôleur Jasmin",     L"Jasmin-Controller",     L"Controladora Jasmin" },
    { L"Jasmin EPROM",         L"EPROM Jasmin",          L"Jasmin-EPROM",          L"EPROM Jasmin" },
    { L"Byte Drive 500 Controller", L"Contrôleur Byte Drive 500", L"Byte Drive 500-Controller", L"Controladora Byte Drive 500" },
    { L"BD-500 EPROM",         L"EPROM BD-500",          L"BD-500-EPROM",          L"EPROM BD-500" },
    { L"Oric Telestrat",       L"Oric Telestrat",        L"Oric Telestrat",        L"Oric Telestrat" },
    { L"Telemon v2.4 (Bank 7)",L"Telemon v2.4 (Banque 7)",L"Telemon v2.4 (Bank 7)",L"Telemon v2.4 (Banco 7)" },
    { L"HyperBasic (Bank 6)",  L"HyperBasic (Banque 6)", L"HyperBasic (Bank 6)", L"HyperBasic (Banco 6)" },
    { L"Tele-Ass (Bank 5)",    L"Télé-Ass (Banque 5)",   L"Tele-Ass (Bank 5)",   L"Tele-Ass (Banco 5)" },
    { L"Pravetz 8D (Oric Clone)", L"Pravetz 8D (Clone Oric)", L"Pravetz 8D (Oric-Klon)", L"Pravetz 8D (Clon Oric)" },
    { L"Pravetz 8D Monitor/BASIC",L"Moniteur/BASIC Pravetz 8D",L"Pravetz 8D Monitor/BASIC",L"Monitor/BASIC Pravetz 8D" },
    { L"Standard ASCII",       L"ASCII standard",        L"Standard-ASCII",        L"ASCII estándar" },
    { L"French / AZERTY patch",L"Patch Français / AZERTY",L"Französischer AZERTY-Patch",L"Parche Francés / AZERTY" },
    { L"Cyrillic (Pravetz 8D)",L"Cyrillique (Pravetz 8D)",L"Kyrillisch (Pravetz 8D)",L"Cirílico (Pravetz 8D)" },
    {
        L"  (No recognized ASCII signature strings)",
        L"  (Aucune chaîne signature ASCII reconnue)",
        L"  (Keine bekannten ASCII-Signaturzeichenketten gefunden)",
        L"  (No se encontraron cadenas de firma ASCII reconocidas)"
    },
    {
        L"YES - Original ROM (%ls)",
        L"OUI - ROM d'origine (%ls)",
        L"JA - Original-ROM (%ls)",
        L"SÍ - ROM original (%ls)"
    },
    {
        L"Oric Atmos Base (modified / custom)",
        L"Base Oric Atmos (modifiée / custom)",
        L"Oric Atmos Basis (modifiziert / custom)",
        L"Base Oric Atmos (modificada / personalizada)"
    },
    { L"Derived BASIC v1.1", L"Dérivée BASIC v1.1", L"Abgeleitetes BASIC v1.1", L"Derivada de BASIC v1.1" },
    {
        L"Oric-1 Base (modified / custom)",
        L"Base Oric-1 (modifiée / custom)",
        L"Oric-1 Basis (modifiziert / custom)",
        L"Base Oric-1 (modificada / personalizada)"
    },
    { L"Derived BASIC v1.0", L"Dérivée BASIC v1.0", L"Abgeleitetes BASIC v1.0", L"Derivada de BASIC v1.0" },
    {
        L"Non-standard 16 KB Oric ROM",
        L"ROM Oric 16 Ko non standard",
        L"Nicht standardmäßiges 16-KB-Oric-ROM",
        L"ROM Oric de 16 KB no estándar"
    },
    { L"Custom version", L"Version personnalisée", L"Benutzerdefinierte Version", L"Versión personalizada" },
    {
        L"NO - Modified / non-original ROM (CRC mismatch)",
        L"NON - ROM modifiée / non originale (CRC différent)",
        L"NEIN - Modifiziertes / kein Original-ROM (CRC weicht ab)",
        L"NO - ROM modificada / no original (CRC distinto)"
    },
    {
        L"Unlisted ROM / Binary",
        L"ROM / Binaire non répertorié",
        L"Nicht gelistetes ROM / Binärdatei",
        L"ROM / Binario no catalogado"
    },
    { L"Unknown", L"Inconnue", L"Unbekannt", L"Desconocida" },
    {
        L"NO - Checksum not in official Oric database",
        L"NON - Empreinte absente de la base officielle Oric",
        L"NEIN - Prüfsumme nicht in offizieller Oric-Datenbank",
        L"NO - Suma de verificación ausente de la base oficial Oric"
    },
    { L"ROM Identification", L"Identification ROM", L"ROM-Identifikation", L"Identificación de ROM" },
    { L"Authenticity",       L"Authenticité",       L"Echtheit",           L"Autenticidad" },
    { L"Original ROM",       L"ROM d'origine",      L"Original-ROM",       L"ROM original" },
    { L"Unofficial",         L"Non officielle",     L"Nicht offiziell",    L"No oficial" },
    { L"Model / Version",    L"Modèle / Version",   L"Modell / Version",   L"Modelo / Versión" },
    { L"NMI Vector ($FFFA)", L"Vecteur NMI ($FFFA)",L"NMI-Vektor ($FFFA)", L"Vector NMI ($FFFA)" },
    { L"RESET Vector ($FFFC)",L"Vecteur RESET ($FFFC)",L"RESET-Vektor ($FFFC)",L"Vector RESET ($FFFC)" },
    { L"IRQ/BRK Vector ($FFFE)",L"Vecteur IRQ/BRK ($FFFE)",L"IRQ/BRK-Vektor ($FFFE)",L"Vector IRQ/BRK ($FFFE)" },
    { L"6502 Vector",        L"Vecteur 6502",       L"6502-Vektor",        L"Vector 6502" },
    { L"Non-maskable interrupt", L"Interruption non masquable", L"Nicht maskierbarer Interrupt", L"Interrupción no enmascarable" },
    { L"Valid entry point",  L"Point d'entrée valide", L"Gültiger Einsprungpunkt", L"Punto de entrada válido" },
    { L"Outside ROM range",  L"Hors plage ROM",     L"Außerhalb ROM-Bereich", L"Fuera de rango ROM" },
    { L"Hardware interrupt", L"Interruption matérielle", L"Hardware-Interrupt", L"Interrupción hardware" },
    {
        L"ROM analyzed: %ls (CRC32 0x%08X)",
        L"ROM analysée : %ls (CRC32 0x%08X)",
        L"ROM analysiert: %ls (CRC32 0x%08X)",
        L"ROM analizada: %ls (CRC32 0x%08X)"
    },

    /* Summary & Previews */
    {
        L"ORIC EXPLORER\r\n"
        L"========================================\r\n\r\n"
        L"No file loaded.\r\n\r\n"
        L"Drag and drop a file directly onto\r\n"
        L"the window or use the File Explorer.\r\n\r\n"
        L"Supported formats:\r\n"
        L"  • Oric Tapes (.tap)\r\n"
        L"  • Floppy Disks (.dsk: SEDORIC, StratSED,\r\n"
        L"    FT-DOS, OricDOS)\r\n"
        L"  • Oric-1, Atmos, Telestrat & Pravetz ROMs\r\n",

        L"ORIC EXPLORER\r\n"
        L"========================================\r\n\r\n"
        L"Aucun fichier chargé.\r\n\r\n"
        L"Glissez-déposez un fichier ou utilisez\r\n"
        L"l'explorateur de fichiers intégré.\r\n\r\n"
        L"Formats pris en charge :\r\n"
        L"  • Cassettes Oric (.tap)\r\n"
        L"  • Disquettes (.dsk : SEDORIC, StratSED,\r\n"
        L"    FT-DOS, OricDOS)\r\n"
        L"  • ROMs Oric-1, Atmos, Telestrat & Pravetz\r\n",

        L"ORIC EXPLORER\r\n"
        L"========================================\r\n\r\n"
        L"Keine Datei geladen.\r\n\r\n"
        L"Ziehen Sie eine Datei ins Fenster oder\r\n"
        L"nutzen Sie den integrierten Explorer.\r\n\r\n"
        L"Unterstützte Formate:\r\n"
        L"  • Oric-Kassetten (.tap)\r\n"
        L"  • Disketten (.dsk: SEDORIC, StratSED,\r\n"
        L"    FT-DOS, OricDOS)\r\n"
        L"  • Oric-1, Atmos, Telestrat & Pravetz ROMs\r\n",

        L"ORIC EXPLORER\r\n"
        L"========================================\r\n\r\n"
        L"Ningún archivo cargado.\r\n\r\n"
        L"Arrastre un archivo a la ventana o use\r\n"
        L"el explorador integrado.\r\n\r\n"
        L"Formatos compatibles:\r\n"
        L"  • Cintas Oric (.tap)\r\n"
        L"  • Discos (.dsk: SEDORIC, StratSED,\r\n"
        L"    FT-DOS, OricDOS)\r\n"
        L"  • ROMs Oric-1, Atmos, Telestrat y Pravetz\r\n"
    },
    /* STR_SUM_HEADER_FMT */
    {
        L"FILE SUMMARY\r\n"
        L"========================================\r\n"
        L"File   : %ls\r\n"
        L"Size   : %u bytes (%.2f KB)\r\n",

        L"RÉSUMÉ DU FICHIER\r\n"
        L"========================================\r\n"
        L"Fichier : %ls\r\n"
        L"Taille  : %u octets (%.2f Ko)\r\n",

        L"DATEIZUSAMMENFASSUNG\r\n"
        L"========================================\r\n"
        L"Datei  : %ls\r\n"
        L"Größe  : %u Bytes (%.2f KB)\r\n",

        L"RESUMEN DEL ARCHIVO\r\n"
        L"========================================\r\n"
        L"Archivo : %ls\r\n"
        L"Tamaño  : %u bytes (%.2f KB)\r\n"
    },
    /* STR_SUM_TAP_FMT */
    {
        L"Format : Oric Tape (.tap)\r\n"
        L"Blocks : %zu program(s)\r\n"
        L"Payload: %llu bytes\r\n",

        L"Format  : Cassette Oric (.tap)\r\n"
        L"Blocs   : %zu programme(s)\r\n"
        L"Utile   : %llu octets\r\n",

        L"Format : Oric-Kassette (.tap)\r\n"
        L"Blöcke : %zu Programm(e)\r\n"
        L"Nutzdat: %llu Bytes\r\n",

        L"Formato : Cinta Oric (.tap)\r\n"
        L"Bloques : %zu programa(s)\r\n"
        L"Útil    : %llu bytes\r\n"
    },
    /* STR_SUM_DSK_FMT */
    {
        L"Format : Oric Disk (%ls)\r\n"
        L"System : %ls\r\n"
        L"Volume : %ls\r\n"
        L"Geom.  : %u side(s), %u tracks, %u sect./track\r\n"
        L"Files  : %zu entry(s)\r\n",

        L"Format  : Disquette Oric (%ls)\r\n"
        L"Système : %ls\r\n"
        L"Volume  : %ls\r\n"
        L"Géom.   : %u face(s), %u pistes, %u sect./piste\r\n"
        L"Fichiers: %zu entrée(s)\r\n",

        L"Format : Oric-Diskette (%ls)\r\n"
        L"System : %ls\r\n"
        L"Volume : %ls\r\n"
        L"Geom.  : %u Seite(n), %u Spuren, %u Sekt./Spur\r\n"
        L"Dateien: %zu Eintrag/Einträge\r\n",

        L"Formato : Disco Oric (%ls)\r\n"
        L"Sistema : %ls\r\n"
        L"Volumen : %ls\r\n"
        L"Geom.   : %u cara(s), %u pistas, %u sect./pista\r\n"
        L"Archivos: %zu entrada(s)\r\n"
    },
    /* STR_SUM_DSK_FREE_FMT */
    {
        L"Space  : %u / %u free sectors\r\n",
        L"Espace  : %u / %u secteurs libres\r\n",
        L"Frei   : %u / %u freie Sektoren\r\n",
        L"Espacio : %u / %u sectores libres\r\n"
    },
    /* STR_SUM_ROM_PART1_FMT */
    {
        L"Format : 6502 ROM Image\r\n\r\n"
        L"AUTHENTICITY\r\n"
        L"----------------------------------------\r\n"
        L"Origin : %ls\r\n"
        L"Model  : %ls\r\n"
        L"Version: %ls\r\n\r\n"
        L"TECHNICAL DETAILS\r\n"
        L"----------------------------------------\r\n"
        L"CRC32  : 0x%08X\r\n"
        L"Size   : %u bytes (%u KB)\r\n"
        L"Range  : $%04X - $FFFF\r\n",

        L"Format  : Image ROM 6502\r\n\r\n"
        L"AUTHENTICITÉ\r\n"
        L"----------------------------------------\r\n"
        L"Origine : %ls\r\n"
        L"Modèle  : %ls\r\n"
        L"Version : %ls\r\n\r\n"
        L"CARACTÉRISTIQUES TECHNIQUES\r\n"
        L"----------------------------------------\r\n"
        L"CRC32   : 0x%08X\r\n"
        L"Taille  : %u octets (%u Ko)\r\n"
        L"Plage   : $%04X - $FFFF\r\n",

        L"Format : 6502-ROM-Image\r\n\r\n"
        L"ECHTHEIT\r\n"
        L"----------------------------------------\r\n"
        L"Herkunft: %ls\r\n"
        L"Modell  : %ls\r\n"
        L"Version : %ls\r\n\r\n"
        L"TECHNISCHE DATEN\r\n"
        L"----------------------------------------\r\n"
        L"CRC32   : 0x%08X\r\n"
        L"Größe   : %u Bytes (%u KB)\r\n"
        L"Bereich : $%04X - $FFFF\r\n",

        L"Formato : Imagen ROM 6502\r\n\r\n"
        L"AUTENTICIDAD\r\n"
        L"----------------------------------------\r\n"
        L"Origen  : %ls\r\n"
        L"Modelo  : %ls\r\n"
        L"Versión : %ls\r\n\r\n"
        L"DETALLES TÉCNICOS\r\n"
        L"----------------------------------------\r\n"
        L"CRC32   : 0x%08X\r\n"
        L"Tamaño  : %u bytes (%u KB)\r\n"
        L"Rango   : $%04X - $FFFF\r\n"
    },
    /* STR_SUM_ROM_CHARSET_FMT */
    {
        L"Charset: %ls\r\n",
        L"Jeu car.: %ls\r\n",
        L"Zeichens: %ls\r\n",
        L"Alfabeto: %ls\r\n"
    },
    /* STR_SUM_ROM_VECTORS_FMT */
    {
        L"NMI Vector   ($FFFA) : $%04X\r\n"
        L"RESET Vector ($FFFC) : $%04X\r\n"
        L"IRQ Vector   ($FFFE) : $%04X\r\n",

        L"Vecteur NMI   ($FFFA) : $%04X\r\n"
        L"Vecteur RESET ($FFFC) : $%04X\r\n"
        L"Vecteur IRQ   ($FFFE) : $%04X\r\n",

        L"NMI-Vektor   ($FFFA) : $%04X\r\n"
        L"RESET-Vektor ($FFFC) : $%04X\r\n"
        L"IRQ-Vektor   ($FFFE) : $%04X\r\n",

        L"Vector NMI   ($FFFA) : $%04X\r\n"
        L"Vector RESET ($FFFC) : $%04X\r\n"
        L"Vector IRQ   ($FFFE) : $%04X\r\n"
    },
    /* STR_SUM_ROM_STRINGS_FMT */
    {
        L"\r\nDETECTED STRINGS\r\n"
        L"----------------------------------------\r\n"
        L"%ls\r\n",

        L"\r\nCHAÎNES DÉTECTÉES\r\n"
        L"----------------------------------------\r\n"
        L"%ls\r\n",

        L"\r\nERKANNTE ZEICHENKETTEN\r\n"
        L"----------------------------------------\r\n"
        L"%ls\r\n",

        L"\r\nCADENAS DETECTADAS\r\n"
        L"----------------------------------------\r\n"
        L"%ls\r\n"
    },
    /* STR_SUM_UNKNOWN_FMT */
    {
        L"Format : Unrecognized\r\nNote   : %ls\r\n",
        L"Format  : Non reconnu\r\nNote    : %ls\r\n",
        L"Format : Unbekannt\r\nHinweis: %ls\r\n",
        L"Formato : No reconocido\r\nNota    : %ls\r\n"
    },
    /* STR_SUM_SEL_HEADER_FMT */
    {
        L"\r\nSELECTED ITEM\r\n"
        L"----------------------------------------\r\n"
        L"Name      : %ls\r\n"
        L"Type      : %ls\r\n"
        L"Size      : %u bytes\r\n",

        L"\r\nÉLÉMENT SÉLECTIONNÉ\r\n"
        L"----------------------------------------\r\n"
        L"Nom       : %ls\r\n"
        L"Type      : %ls\r\n"
        L"Taille    : %u octets\r\n",

        L"\r\nAUSGEWÄHLTES ELEMENT\r\n"
        L"----------------------------------------\r\n"
        L"Name      : %ls\r\n"
        L"Typ       : %ls\r\n"
        L"Größe     : %u Bytes\r\n",

        L"\r\nELEMENTO SELECCIONADO\r\n"
        L"----------------------------------------\r\n"
        L"Nombre    : %ls\r\n"
        L"Tipo      : %ls\r\n"
        L"Tamaño    : %u bytes\r\n"
    },
    /* STR_SUM_SEL_MEM_FMT */
    {
        L"Memory    : $%04X - $%04X\r\n",
        L"Mémoire   : $%04X - $%04X\r\n",
        L"Speicher  : $%04X - $%04X\r\n",
        L"Memoria   : $%04X - $%04X\r\n"
    },
    /* STR_SUM_SEL_EXEC_FMT */
    {
        L"Execution : $%04X\r\n",
        L"Exécution : $%04X\r\n",
        L"Ausführung: $%04X\r\n",
        L"Ejecución : $%04X\r\n"
    },
    /* STR_SUM_SEL_CRC_FMT */
    {
        L"CRC32     : 0x%08X\r\n",
        L"CRC32     : 0x%08X\r\n",
        L"CRC32     : 0x%08X\r\n",
        L"CRC32     : 0x%08X\r\n"
    },
    /* STR_SUM_SEL_TAIL_FMT */
    {
        L"Attributes: %ls\r\nDetails   : %ls\r\n",
        L"Attributs : %ls\r\nDétails   : %ls\r\n",
        L"Attribute : %ls\r\nDetails   : %ls\r\n",
        L"Atributos : %ls\r\nDetalles  : %ls\r\n"
    },

    /* STR_CODE_PLACEHOLDER */
    {
        L"Select a program/file in the list or load a ROM to view BASIC source, 6502 disassembly, or Hex dump.",
        L"Sélectionnez un programme/fichier dans la liste ou chargez une ROM pour afficher le source BASIC, le désassemblage 6502 ou le dump Hexa.",
        L"Wählen Sie ein Programm/eine Datei oder laden Sie ein ROM für BASIC-Quellcode, 6502-Disassembly oder Hex-Dump.",
        L"Seleccione un archivo o cargue una ROM para ver código BASIC, desensamblado 6502 o volcado Hex."
    },
    {
        L"ORIC BASIC LISTING: %ls (%zu bytes)\r\n========================================\r\n\r\n",
        L"LISTING BASIC ORIC : %ls (%zu octets)\r\n========================================\r\n\r\n",
        L"ORIC BASIC-LISTING: %ls (%zu Bytes)\r\n========================================\r\n\r\n",
        L"LISTADO BASIC ORIC: %ls (%zu bytes)\r\n========================================\r\n\r\n"
    },
    {
        L"6502 DISASSEMBLY: %ls (Base $%04X, %zu bytes)\r\n========================================\r\n\r\n",
        L"DÉSASSEMBLAGE 6502 : %ls (Base $%04X, %zu octets)\r\n========================================\r\n\r\n",
        L"6502-DISASSEMBLY: %ls (Basis $%04X, %zu Bytes)\r\n========================================\r\n\r\n",
        L"DESENSAMBLADO 6502: %ls (Base $%04X, %zu bytes)\r\n========================================\r\n\r\n"
    },
    {
        L"HEX / ASCII DUMP: %ls (Base $%04X, %zu bytes)\r\n========================================\r\n\r\n",
        L"DUMP HEXA / ASCII : %ls (Base $%04X, %zu octets)\r\n========================================\r\n\r\n",
        L"HEX / ASCII DUMP: %ls (Basis $%04X, %zu Bytes)\r\n========================================\r\n\r\n",
        L"VOLCADO HEX / ASCII: %ls (Base $%04X, %zu bytes)\r\n========================================\r\n\r\n"
    },
    {
        L"No HIRES/TEXT screen or disk map to display.\r\n\r\nSelect a HIRES (.hir / $A000) or TEXT ($BB80) entry, or open a .dsk image.",
        L"Aucun écran HIRES/TEXT ou carte disquette à afficher.\r\n\r\nSélectionnez un fichier HIRES (.hir / $A000) ou TEXT ($BB80), ou ouvrez une disquette .dsk.",
        L"Kein HIRES/TEXT-Bildschirm oder Diskettenkarte.\r\n\r\nWählen Sie eine HIRES/TEXT-Datei oder öffnen Sie ein .dsk-Image.",
        L"No hay pantalla HIRES/TEXT ni mapa de disco.\r\n\r\nSeleccione un archivo HIRES/TEXT o abra una imagen .dsk."
    },
    {
        L"%ls — %ux%u (%ls)",
        L"%ls — %ux%u (%ls)",
        L"%ls — %ux%u (%ls)",
        L"%ls — %ux%u (%ls)"
    },
    {
        L"Disk Sector Map (%ls) — %u side(s), %u trk x %u sec",
        L"Carte secteurs (%ls) — %u face(s), %u pistes x %u sect.",
        L"Sektorkarte (%ls) — %u Seite(n), %u Sp. x %u Sekt.",
        L"Mapa sectores (%ls) — %u cara(s), %u pistas x %u sect."
    },
    {
        L"Free  Used  System  Selected",
        L"Libre  Occupé  Système  Sélection",
        L"Frei  Belegt  System  Auswahl",
        L"Libre  Usado  Sistema  Selección"
    }
};

const wchar_t *oric_tr(OricLang lang, OricStrId id) {
    if ((int)lang < 0 || lang >= ORIC_LANG_COUNT) {
        lang = ORIC_LANG_EN;
    }
    if ((int)id < 0 || id >= STR_ID_COUNT) {
        return L"";
    }
    return TRANSLATIONS[id][lang];
}

const wchar_t *oric_lang_code(OricLang lang) {
    switch (lang) {
    case ORIC_LANG_FR: return L"fr";
    case ORIC_LANG_DE: return L"de";
    case ORIC_LANG_ES: return L"es";
    case ORIC_LANG_EN:
    default:           return L"en";
    }
}

OricLang oric_lang_from_code(const wchar_t *code) {
    if (!code) return ORIC_LANG_EN;
    if (wcscmp(code, L"fr") == 0) return ORIC_LANG_FR;
    if (wcscmp(code, L"de") == 0) return ORIC_LANG_DE;
    if (wcscmp(code, L"es") == 0) return ORIC_LANG_ES;
    return ORIC_LANG_EN;
}
