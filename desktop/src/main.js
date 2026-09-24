const { invoke } = window.__TAURI__.core;

const appToggle = document.querySelector("#app-toggle");
const wallToggle = document.querySelector("#wall-toggle");
const changeButton = document.querySelector("#change");

function paint(state) {
  appToggle.checked = state.appEnabled;
  wallToggle.checked = state.wallpaperEnabled;
  document.querySelector("#title").textContent = state.title;
  document.querySelector("#thought").textContent = state.thought;
  document.querySelector("#filename").textContent = state.filename;
  document.querySelector("#note").textContent = state.note || "";
  if (state.imageBase64) {
    document.querySelector("#preview").src = `data:image/png;base64,${state.imageBase64}`;
  }
}

async function call(name, args) {
  changeButton.disabled = true;
  try {
    paint(await invoke(name, args));
  } catch (error) {
    document.querySelector("#note").textContent = String(error);
  } finally {
    changeButton.disabled = false;
  }
}

appToggle.addEventListener("change", () => {
  call("set_toggles", {
    appEnabled: appToggle.checked,
    wallpaperEnabled: wallToggle.checked,
  });
});

wallToggle.addEventListener("change", () => {
  call("set_toggles", {
    appEnabled: appToggle.checked,
    wallpaperEnabled: wallToggle.checked,
  });
});

changeButton.addEventListener("click", () => call("change_now"));

window.addEventListener("DOMContentLoaded", () => call("get_state"));
