#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define MAX_WALLS 128
#define MAX_LINE 1024

typedef struct {
    const char *file;
    const char *titel;
    const char *gedanke;
} Eintrag;

static const Eintrag KATALOG[] = {
    {"ax-01-kirschbluete.png", "Kirschblüte",
     "Was langsam fällt, muss nicht verloren gehen. Manches bleibt, weil du stehen bleibst."},
    {"ax-02-neonregen.png", "Neonregen",
     "Auch eine laute Stadt hat stille Ecken. Such dir eine, und atme, bis der Regen nur noch Regen ist."},
    {"ax-03-wolkentempel.png", "Wolkentempel",
     "Du musst nicht alles sehen, um anzukommen. Manchmal reicht ein Licht über den Wolken."},
    {"ax-04-leuchtturm.png", "Leuchtturm",
     "Ein kleines Licht reicht weit, wenn das Meer dunkel ist. Sei dieses Licht, auch für dich selbst."},
    {"ax-05-nachtzug.png", "Nachtzug",
     "Nicht jeder Weg braucht ein Ziel in dieser Stunde. Manche Nächte sind nur dazu da, dich weiterzutragen."},
    {"ax-06-ahornwald.png", "Ahornwald",
     "Loslassen kann bunt sein. Was du hergibst, macht Platz für das, was als Nächstes wachsen will."},
    {"ax-07-dachgarten.png", "Dachgarten",
     "Von oben sieht das Durcheinander kleiner aus. Steig hin und wieder hoch, nur um zu schauen."},
    {"ax-08-schneedorf.png", "Schneedorf",
     "Wärme ist kein Ort, sondern jemand, der das Fenster für dich anlässt."},
    {"ax-09-inselreich.png", "Inselreich",
     "Deine Welt darf größer sein als dein Tag. Lass einen Rand offen, an dem noch Inseln Platz haben."},
    {"ax-10-bibliothek.png", "Bibliothek",
     "Du musst nicht jede Seite heute lesen. Es genügt, das Buch aufzuschlagen und dazubleiben."},
    {"ax-11-sommerfest.png", "Sommerfest",
     "Freude wird nicht kleiner, wenn du sie teilst. Sie wird nur leichter zu tragen."},
    {"ax-12-regencafe.png", "Regencafé",
     "Pause ist kein Rückschritt. Ein warmer Tisch am Fenster zählt auch als Weiterkommen."},
};

static int shell_quote(const char *in, char *out, size_t out_sz) {
    size_t j = 0;
    if (out_sz < 3) return -1;
    out[j++] = '\'';
    for (size_t i = 0; in[i]; i++) {
        if (in[i] == '\'') {
            if (j + 4 >= out_sz) return -1;
            memcpy(out + j, "'\\''", 4);
            j += 4;
        } else {
            if (j + 1 >= out_sz) return -1;
            out[j++] = in[i];
        }
    }
    if (j + 2 > out_sz) return -1;
    out[j++] = '\'';
    out[j] = '\0';
    return 0;
}

static int have(const char *cmd) {
    char buf[512];
    snprintf(buf, sizeof(buf), "command -v %s >/dev/null 2>&1", cmd);
    return system(buf) == 0;
}

static int run(const char *cmd) {
    int rc = system(cmd);
    return rc == 0;
}

static void ensure_dir(const char *path) {
    char tmp[PATH_MAX];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

static int exe_dir(char *dir, size_t n) {
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len < 0) return -1;
    buf[len] = '\0';
    char *slash = strrchr(buf, '/');
    if (!slash) return -1;
    *slash = '\0';
    if (strlen(buf) + 1 > n) return -1;
    memcpy(dir, buf, strlen(buf) + 1);
    return 0;
}

static const Eintrag *finde(const char *datei) {
    size_t n = sizeof(KATALOG) / sizeof(KATALOG[0]);
    for (size_t i = 0; i < n; i++) {
        if (strcmp(KATALOG[i].file, datei) == 0) return &KATALOG[i];
    }
    return NULL;
}

static int lade_waende(const char *ordner, char pfade[][PATH_MAX], char namen[][NAME_MAX], int max) {
    DIR *d = opendir(ordner);
    if (!d) return -1;
    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL && count < max) {
        const char *name = ent->d_name;
        size_t len = strlen(name);
        if (len < 5) continue;
        if (strcasecmp(name + len - 4, ".png") != 0 &&
            strcasecmp(name + len - 4, ".jpg") != 0 &&
            (len < 5 || strcasecmp(name + len - 5, ".jpeg") != 0)) {
            continue;
        }
        snprintf(namen[count], NAME_MAX, "%s", name);
        snprintf(pfade[count], PATH_MAX, "%s/%s", ordner, name);
        count++;
    }
    closedir(d);
    for (int i = 0; i < count; i++) {
        for (int j = i + 1; j < count; j++) {
            if (strcmp(namen[j], namen[i]) < 0) {
                char ts[NAME_MAX], tp[PATH_MAX];
                memcpy(ts, namen[i], NAME_MAX);
                memcpy(namen[i], namen[j], NAME_MAX);
                memcpy(namen[j], ts, NAME_MAX);
                memcpy(tp, pfade[i], PATH_MAX);
                memcpy(pfade[i], pfade[j], PATH_MAX);
                memcpy(pfade[j], tp, PATH_MAX);
            }
        }
    }
    return count;
}

static int lies_letzte(const char *state, char *out, size_t n) {
    FILE *f = fopen(state, "r");
    if (!f) return -1;
    if (!fgets(out, (int)n, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    size_t len = strlen(out);
    while (len && (out[len - 1] == '\n' || out[len - 1] == '\r')) out[--len] = '\0';
    return 0;
}

static void schreibe_letzte(const char *state, const char *name) {
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s", state);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        ensure_dir(dir);
    }
    FILE *f = fopen(state, "w");
    if (!f) return;
    fprintf(f, "%s\n", name);
    fclose(f);
}

static int setze_hintergrund(const char *pfad, int trocken) {
    char q[PATH_MAX * 2];
    if (shell_quote(pfad, q, sizeof(q)) != 0) return 0;
    char uri[PATH_MAX * 2 + 16];
    snprintf(uri, sizeof(uri), "file://%s", pfad);
    char qu[PATH_MAX * 2 + 32];
    if (shell_quote(uri, qu, sizeof(qu)) != 0) return 0;

    if (trocken) {
        printf("Trockenlauf: würde setzen auf %s\n", pfad);
        return 1;
    }

    int ok = 0;
    char cmd[PATH_MAX * 4];

    if (have("gsettings")) {
        const char *schemas[] = {
            "org.gnome.desktop.background",
            "org.cinnamon.desktop.background",
            "org.mate.background",
            NULL
        };
        for (int i = 0; schemas[i]; i++) {
            if (strcmp(schemas[i], "org.mate.background") == 0) {
                snprintf(cmd, sizeof(cmd),
                         "gsettings set %s picture-filename %s >/dev/null 2>&1",
                         schemas[i], q);
                if (run(cmd)) ok = 1;
            } else {
                snprintf(cmd, sizeof(cmd),
                         "gsettings set %s picture-uri %s >/dev/null 2>&1",
                         schemas[i], qu);
                if (run(cmd)) ok = 1;
                snprintf(cmd, sizeof(cmd),
                         "gsettings set %s picture-uri-dark %s >/dev/null 2>&1",
                         schemas[i], qu);
                run(cmd);
                snprintf(cmd, sizeof(cmd),
                         "gsettings set %s picture-options 'zoom' >/dev/null 2>&1",
                         schemas[i]);
                run(cmd);
            }
        }
    }

    if (have("xfconf-query")) {
        snprintf(cmd, sizeof(cmd),
                 "props=$(xfconf-query -c xfce4-desktop -l 2>/dev/null | grep -E 'last-image$'); "
                 "if [ -n \"$props\" ]; then echo \"$props\" | while read -r p; do "
                 "xfconf-query -c xfce4-desktop -p \"$p\" -s %s; done; exit 0; else exit 1; fi",
                 q);
        if (run(cmd)) ok = 1;
    }

    if (have("qdbus")) {
        snprintf(cmd, sizeof(cmd),
                 "qdbus org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript "
                 "\"var a=desktops(); for (i=0;i<a.length;i++){d=a[i]; d.wallpaperPlugin='org.kde.image'; "
                 "d.currentConfigGroup=Array('Wallpaper','org.kde.image','General'); "
                 "d.writeConfig('Image', %s);}\" >/dev/null 2>&1",
                 qu);
        if (run(cmd)) ok = 1;
    }

    if (have("swaymsg") && getenv("SWAYSOCK")) {
        snprintf(cmd, sizeof(cmd), "swaymsg output \"*\" bg %s fill >/dev/null 2>&1", q);
        if (run(cmd)) ok = 1;
    }

    if (have("hyprctl") && getenv("HYPRLAND_INSTANCE_SIGNATURE")) {
        snprintf(cmd, sizeof(cmd), "hyprctl hyprpaper preload %s >/dev/null 2>&1 && hyprctl hyprpaper wallpaper \",%s\" >/dev/null 2>&1", q, pfad);
        if (run(cmd)) ok = 1;
    }

    if (have("feh")) {
        snprintf(cmd, sizeof(cmd), "feh --bg-fill %s >/dev/null 2>&1", q);
        if (run(cmd)) ok = 1;
    }

    if (have("nitrogen")) {
        snprintf(cmd, sizeof(cmd), "nitrogen --set-zoom-fill %s >/dev/null 2>&1", q);
        if (run(cmd)) ok = 1;
    }

    if (have("pcmanfm")) {
        snprintf(cmd, sizeof(cmd), "pcmanfm --set-wallpaper %s >/dev/null 2>&1", q);
        if (run(cmd)) ok = 1;
    }

    if (have("xwallpaper")) {
        snprintf(cmd, sizeof(cmd), "xwallpaper --zoom %s >/dev/null 2>&1", q);
        if (run(cmd)) ok = 1;
    }

    return ok;
}

static void zeige_gedanken(const Eintrag *e, const char *datei) {
    const char *titel = e ? e->titel : datei;
    const char *text = e ? e->gedanke : "Heute ein neues Bild. Nimm dir einen Moment, bevor der Tag weitergeht.";
    printf("\n  AnimeWallpaperX\n");
    printf("  Bild: %s\n\n", titel);
    printf("  \"%s\"\n\n", text);

    if (!have("notify-send")) return;
    char qt[512], qg[1024], cmd[1800];
    if (shell_quote(titel, qt, sizeof(qt)) != 0) return;
    if (shell_quote(text, qg, sizeof(qg)) != 0) return;
    snprintf(cmd, sizeof(cmd), "notify-send -a AnimeWallpaperX -i wallpaper %s %s >/dev/null 2>&1", qt, qg);
    run(cmd);
}

static void hilfe(const char *argv0) {
    printf("AnimeWallpaperX\n");
    printf("Wechselt bei jedem Start das Linux-Hintergrundbild und zeigt einen Gedanken.\n\n");
    printf("Aufruf: %s [Option]\n\n", argv0);
    printf("  (ohne Option)     Bild wählen, Hintergrund setzen, Gedanken zeigen\n");
    printf("  --list            Alle mitgelieferten Bilder und Gedanken\n");
    printf("  --dry-run         Nur auswaehlen, nichts am Desktop aendern\n");
    printf("  --autostart       Beim Anmelden automatisch starten\n");
    printf("  --help            Diese Hilfe\n");
}

static int autostart(const char *self) {
    const char *home = getenv("HOME");
    if (!home) home = "/root";
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/.config/autostart", home);
    ensure_dir(dir);
    char file[PATH_MAX];
    snprintf(file, sizeof(file), "%s/AnimeWallpaperX.desktop", dir);
    FILE *f = fopen(file, "w");
    if (!f) {
        perror(file);
        return 1;
    }
    fprintf(f,
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=AnimeWallpaperX\n"
            "Comment=Wechselt beim Anmelden das Hintergrundbild\n"
            "Exec=%s\n"
            "X-GNOME-Autostart-enabled=true\n"
            "Terminal=false\n",
            self);
    fclose(f);
    printf("Autostart eingerichtet: %s\n", file);
    return 0;
}

int main(int argc, char **argv) {
    int trocken = 0;
    int liste = 0;
    int auto_flag = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            hilfe(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            trocken = 1;
        } else if (strcmp(argv[i], "--list") == 0) {
            liste = 1;
        } else if (strcmp(argv[i], "--autostart") == 0) {
            auto_flag = 1;
        } else {
            fprintf(stderr, "Unbekannte Option: %s\n", argv[i]);
            hilfe(argv[0]);
            return 2;
        }
    }

    char dir[PATH_MAX];
    if (exe_dir(dir, sizeof(dir)) != 0) {
        fprintf(stderr, "Programmpfad nicht gefunden.\n");
        return 1;
    }

    char self[PATH_MAX];
    ssize_t sl = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (sl < 0) {
        fprintf(stderr, "Programmpfad nicht gefunden.\n");
        return 1;
    }
    self[sl] = '\0';
    if (auto_flag) return autostart(self);

    char walls[PATH_MAX];
    snprintf(walls, sizeof(walls), "%s/wallpapers", dir);

    char pfade[MAX_WALLS][PATH_MAX];
    char namen[MAX_WALLS][NAME_MAX];
    int n = lade_waende(walls, pfade, namen, MAX_WALLS);
    if (n <= 0) {
        fprintf(stderr, "Keine Bilder in %s\n", walls);
        return 1;
    }

    if (liste) {
        for (int i = 0; i < n; i++) {
            const Eintrag *e = finde(namen[i]);
            printf("%s\n  %s\n", namen[i], e ? e->gedanke : "(ohne Gedanken)");
        }
        return 0;
    }

    const char *home = getenv("HOME");
    if (!home) home = "/root";
    char state[PATH_MAX];
    snprintf(state, sizeof(state), "%s/.config/AnimeWallpaperX/letzte", home);
    char letzte[NAME_MAX] = "";
    lies_letzte(state, letzte, sizeof(letzte));

    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    int idx = rand() % n;
    if (n > 1 && letzte[0]) {
        for (int versuch = 0; versuch < 16; versuch++) {
            idx = rand() % n;
            if (strcmp(namen[idx], letzte) != 0) break;
        }
    }

    int gesetzt = setze_hintergrund(pfade[idx], trocken);
    schreibe_letzte(state, namen[idx]);
    zeige_gedanken(finde(namen[idx]), namen[idx]);

    if (!gesetzt && !trocken) {
        printf("Hinweis: Kein Desktop erkannt. Starte das Programm in einer grafischen Sitzung\n");
        printf("(GNOME, KDE, XFCE, Sway, i3 mit feh). Bild wurde trotzdem gewaehlt:\n%s\n", pfade[idx]);
        return 0;
    }
    if (gesetzt && !trocken) {
        printf("Hintergrund gesetzt: %s\n", pfade[idx]);
    }
    return 0;
}
