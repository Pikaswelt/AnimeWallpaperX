use base64::Engine;
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::time::{SystemTime, UNIX_EPOCH};
use tauri::Manager;

struct Card {
    file: &'static str,
    title: &'static str,
    thought: &'static str,
}

const CARDS: &[Card] = &[
    Card { file: "ax-01-kirschbluete.png", title: "Kirschblüte", thought: "Was langsam fällt, muss nicht verloren gehen. Manches bleibt, weil du stehen bleibst." },
    Card { file: "ax-02-neonregen.png", title: "Neonregen", thought: "Auch eine laute Stadt hat stille Ecken. Such dir eine, und atme, bis der Regen nur noch Regen ist." },
    Card { file: "ax-03-wolkentempel.png", title: "Wolkentempel", thought: "Du musst nicht alles sehen, um anzukommen. Manchmal reicht ein Licht über den Wolken." },
    Card { file: "ax-04-leuchtturm.png", title: "Leuchtturm", thought: "Ein kleines Licht reicht weit, wenn das Meer dunkel ist. Sei dieses Licht, auch für dich selbst." },
    Card { file: "ax-05-nachtzug.png", title: "Nachtzug", thought: "Nicht jeder Weg braucht ein Ziel in dieser Stunde. Manche Nächte sind nur dazu da, dich weiterzutragen." },
    Card { file: "ax-06-ahornwald.png", title: "Ahornwald", thought: "Loslassen kann bunt sein. Was du hergibst, macht Platz für das, was als Nächstes wachsen will." },
    Card { file: "ax-07-dachgarten.png", title: "Dachgarten", thought: "Von oben sieht das Durcheinander kleiner aus. Steig hin und wieder hoch, nur um zu schauen." },
    Card { file: "ax-08-schneedorf.png", title: "Schneedorf", thought: "Wärme ist kein Ort, sondern jemand, der das Fenster für dich anlässt." },
    Card { file: "ax-09-inselreich.png", title: "Inselreich", thought: "Deine Welt darf größer sein als dein Tag. Lass einen Rand offen, an dem noch Inseln Platz haben." },
    Card { file: "ax-10-bibliothek.png", title: "Bibliothek", thought: "Du musst nicht jede Seite heute lesen. Es genügt, das Buch aufzuschlagen und dazubleiben." },
    Card { file: "ax-11-sommerfest.png", title: "Sommerfest", thought: "Freude wird nicht kleiner, wenn du sie teilst. Sie wird nur leichter zu tragen." },
    Card { file: "ax-12-regencafe.png", title: "Regencafé", thought: "Pause ist kein Rückschritt. Ein warmer Tisch am Fenster zählt auch als Weiterkommen." },
];

#[derive(Serialize, Deserialize, Clone)]
struct Settings {
    app_enabled: bool,
    wallpaper_enabled: bool,
}

impl Default for Settings {
    fn default() -> Self {
        Self { app_enabled: true, wallpaper_enabled: true }
    }
}

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
struct View {
    app_enabled: bool,
    wallpaper_enabled: bool,
    title: String,
    thought: String,
    filename: String,
    image_base64: String,
    note: String,
}

fn home() -> PathBuf {
    std::env::var("HOME").map(PathBuf::from).unwrap_or_else(|_| PathBuf::from("/root"))
}

fn config_dir() -> PathBuf {
    home().join(".config/AnimeWallpaperX")
}

fn settings_path() -> PathBuf {
    config_dir().join("settings.json")
}

fn last_path() -> PathBuf {
    config_dir().join("letzte")
}

fn autostart_path() -> PathBuf {
    home().join(".config/autostart/AnimeWallpaperX.desktop")
}

fn load_settings() -> Settings {
    fs::read_to_string(settings_path())
        .ok()
        .and_then(|s| serde_json::from_str(&s).ok())
        .unwrap_or_default()
}

fn save_settings(settings: &Settings) {
    let _ = fs::create_dir_all(config_dir());
    if let Ok(text) = serde_json::to_string_pretty(settings) {
        let _ = fs::write(settings_path(), text);
    }
}

fn read_last() -> String {
    fs::read_to_string(last_path())
        .unwrap_or_default()
        .trim()
        .to_string()
}

fn write_last(name: &str) {
    let _ = fs::create_dir_all(config_dir());
    let _ = fs::write(last_path(), format!("{name}\n"));
}

fn wallpaper_dir(app: &tauri::AppHandle) -> PathBuf {
    if let Ok(dir) = app.path().resource_dir() {
        let bundled = dir.join("wallpapers");
        if bundled.is_dir() {
            return bundled;
        }
    }
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("wallpapers")
}

fn list_images(dir: &Path) -> Vec<String> {
    let mut names = Vec::new();
    let Ok(entries) = fs::read_dir(dir) else {
        return names;
    };
    for entry in entries.flatten() {
        let name = entry.file_name().to_string_lossy().to_string();
        let lower = name.to_lowercase();
        if lower.ends_with(".png") || lower.ends_with(".jpg") || lower.ends_with(".jpeg") {
            names.push(name);
        }
    }
    names.sort();
    names
}

fn card_for(name: &str) -> (&str, &str) {
    CARDS
        .iter()
        .find(|c| c.file == name)
        .map(|c| (c.title, c.thought))
        .unwrap_or(("AnimeWallpaperX", "Heute ein neues Bild. Nimm dir einen Moment, bevor der Tag weitergeht."))
}

fn encode_image(path: &Path) -> String {
    fs::read(path)
        .map(|bytes| base64::engine::general_purpose::STANDARD.encode(bytes))
        .unwrap_or_default()
}

fn have(cmd: &str) -> bool {
    Command::new("sh")
        .args(["-c", &format!("command -v {cmd} >/dev/null 2>&1")])
        .status()
        .map(|s| s.success())
        .unwrap_or(false)
}

fn spawn(cmd: &str, args: &[&str]) -> bool {
    Command::new(cmd).args(args).status().map(|s| s.success()).unwrap_or(false)
}

fn shell_quote(value: &str) -> String {
    format!("'{}'", value.replace('\'', "'\\''"))
}

fn set_wallpaper(path: &Path) -> String {
    let path_s = path.to_string_lossy().to_string();
    let uri = format!("file://{path_s}");
    let mut ok = Vec::new();

    if have("gsettings") {
        for schema in ["org.gnome.desktop.background", "org.cinnamon.desktop.background"] {
            if spawn("gsettings", &["set", schema, "picture-uri", &uri]) {
                ok.push(schema);
            }
            let _ = spawn("gsettings", &["set", schema, "picture-uri-dark", &uri]);
            let _ = spawn("gsettings", &["set", schema, "picture-options", "zoom"]);
        }
        if spawn("gsettings", &["set", "org.mate.background", "picture-filename", &path_s]) {
            ok.push("MATE");
        }
    }

    if have("xfconf-query") {
        let script = format!(
            "props=$(xfconf-query -c xfce4-desktop -l 2>/dev/null | grep 'last-image$'); \
             if [ -z \"$props\" ]; then exit 1; fi; \
             printf '%s\\n' \"$props\" | while IFS= read -r p; do xfconf-query -c xfce4-desktop -p \"$p\" -s {}; done",
            shell_quote(&path_s)
        );
        if spawn("sh", &["-c", &script]) {
            ok.push("XFCE");
        }
    }

    if have("qdbus") {
        let safe = uri.replace('\\', "\\\\").replace('\'', "\\'");
        let script = format!(
            "var a=desktops(); for (i=0;i<a.length;i++) {{ d=a[i]; d.wallpaperPlugin='org.kde.image'; d.currentConfigGroup=Array('Wallpaper','org.kde.image','General'); d.writeConfig('Image','{safe}'); }}"
        );
        if spawn("qdbus", &["org.kde.plasmashell", "/PlasmaShell", "org.kde.PlasmaShell.evaluateScript", &script]) {
            ok.push("KDE");
        }
    }

    if have("swaymsg") && std::env::var("SWAYSOCK").is_ok() && spawn("swaymsg", &["output", "*", "bg", &path_s, "fill"]) {
        ok.push("Sway");
    }
    if have("feh") && spawn("feh", &["--bg-fill", &path_s]) {
        ok.push("feh");
    }
    if have("nitrogen") && spawn("nitrogen", &["--set-zoom-fill", &path_s]) {
        ok.push("nitrogen");
    }
    if have("pcmanfm") && spawn("pcmanfm", &["--set-wallpaper", &path_s]) {
        ok.push("pcmanfm");
    }

    if ok.is_empty() {
        "Kein Desktop erkannt. Die Wahl ist gespeichert und gilt in einer grafischen Sitzung.".to_string()
    } else {
        format!("Hintergrund gesetzt ({})", ok.join(", "))
    }
}

fn pick_next(names: &[String]) -> Option<String> {
    if names.is_empty() {
        return None;
    }
    let last = read_last();
    let tick = SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_nanos()).unwrap_or(1);
    let mut idx = (tick as usize) % names.len();
    if names.len() > 1 {
        for _ in 0..16 {
            if names[idx] != last {
                break;
            }
            idx = (idx + 1) % names.len();
        }
    }
    Some(names[idx].clone())
}

fn sync_autostart(enabled: bool) {
    let path = autostart_path();
    if !enabled {
        let _ = fs::remove_file(&path);
        return;
    }
    let Ok(exe) = std::env::current_exe() else {
        return;
    };
    if let Some(parent) = path.parent() {
        let _ = fs::create_dir_all(parent);
    }
    let body = format!(
        "[Desktop Entry]\nType=Application\nName=AnimeWallpaperX\nComment=Wechselt beim Anmelden das Hintergrundbild\nExec={}\nX-GNOME-Autostart-enabled=true\nTerminal=false\nCategories=Utility;\n",
        exe.display()
    );
    let _ = fs::write(path, body);
}

fn view(app: &tauri::AppHandle, note: String) -> Result<View, String> {
    let settings = load_settings();
    let dir = wallpaper_dir(app);
    let names = list_images(&dir);
    if names.is_empty() {
        return Err(format!("Keine Bilder in {}", dir.display()));
    }
    let last = read_last();
    let filename = if names.iter().any(|n| n == &last) {
        last
    } else {
        names[0].clone()
    };
    let (title, thought) = card_for(&filename);
    let image = encode_image(&dir.join(&filename));
    Ok(View {
        app_enabled: settings.app_enabled,
        wallpaper_enabled: settings.wallpaper_enabled,
        title: title.to_string(),
        thought: thought.to_string(),
        filename,
        image_base64: image,
        note,
    })
}

fn rotate(app: &tauri::AppHandle) -> Result<View, String> {
    let dir = wallpaper_dir(app);
    let names = list_images(&dir);
    let Some(name) = pick_next(&names) else {
        return Err(format!("Keine Bilder in {}", dir.display()));
    };
    let path = dir.join(&name);
    let note = set_wallpaper(&path);
    write_last(&name);
    view(app, note)
}

#[tauri::command]
fn get_state(app: tauri::AppHandle) -> Result<View, String> {
    view(&app, String::new())
}

#[tauri::command]
fn set_toggles(app: tauri::AppHandle, app_enabled: bool, wallpaper_enabled: bool) -> Result<View, String> {
    let previous = load_settings();
    let settings = Settings { app_enabled, wallpaper_enabled };
    save_settings(&settings);
    sync_autostart(app_enabled);
    if wallpaper_enabled && !previous.wallpaper_enabled {
        return rotate(&app);
    }
    let note = if app_enabled {
        "App ist an und startet beim Anmelden.".to_string()
    } else {
        "App ist aus. Beim Anmelden passiert nichts.".to_string()
    };
    let extra = if wallpaper_enabled {
        " Hintergrundwechsel ist an."
    } else {
        " Hintergrundwechsel ist aus."
    };
    view(&app, format!("{note}{extra}"))
}

#[tauri::command]
fn change_now(app: tauri::AppHandle) -> Result<View, String> {
    rotate(&app)
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .setup(|app| {
            let settings = load_settings();
            save_settings(&settings);
            sync_autostart(settings.app_enabled);
            if settings.app_enabled && settings.wallpaper_enabled {
                let _ = rotate(app.handle());
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![get_state, set_toggles, change_now])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
