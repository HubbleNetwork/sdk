// Populate a documentation version selector in the sidebar.
//
// Reads the list of published versions from versions.json at the docs root
// (written by the CI deploy job) and renders a dropdown for switching
// between versions.

(function () {
  "use strict";

  // Set by init() once the DOM is ready. Kept in this scope so renderSelector
  // can use them.
  let basePath = "/";
  let currentVersion = "";
  let pageWithinVersion = "";

  function init() {
    // The docs root path.
    // Injected by layout.html, because each page lives under <root>/<version>/
    // and the script has no other reliable way to know where the root is.
    const meta = document.querySelector('meta[name="docs-base-path"]');
    basePath = meta ? meta.content : "/";

    // Find out which version this is currently on
    const rest = window.location.pathname.startsWith(basePath)
      ? window.location.pathname.slice(basePath.length)
      : "";
    const segments = rest.split("/");
    currentVersion = segments.shift() || "";
    pageWithinVersion = segments.join("/");

    // Fetch the version list
    fetch(basePath + "versions.json")
      .then((resp) => (resp.ok ? resp.json() : Promise.reject(resp.status)))
      .then(renderSelector)
      .catch(() => {
        // No versions.json (e.g. a local build) -> no selector. Fail silently.
      });
  }

  // Convert a branch name into its deployed folder slug,
  // so "release/1.0-branch" -> "release-1.0-branch".
  function slug(branch) {
    return branch.replace(/\//g, "-");
  }

  // Turn a branch name into short display text for the dropdown.
  // e.g. "release/1.0-branch" -> "1.0"
  function label(branch) {
    return branch.replace(/^release\//, "").replace(/-branch$/, "");
  }

  function renderSelector(versions) {
    if (!Array.isArray(versions) || versions.length === 0) {
      return;
    }

    const select = document.createElement("select");
    select.className = "version-selector";
    select.setAttribute("aria-label", "Documentation Version");

    versions.forEach((branch) => {
      const option = document.createElement("option");
      // deployed folder name, used to build the URL
      option.value = slug(branch);

      // prettified display text
      option.textContent = label(branch);

      option.selected = slug(branch) === currentVersion;
      select.appendChild(option);
    });

    select.addEventListener("change", () => {
      // Jump to the same page in the chosen version.
      // The custom 404 page handles the case where that page doesn't exist.
      window.location.href = basePath + select.value + "/" + pageWithinVersion;
    });

    // Build a labeled "Version" section pinned to the bottom of the sidebar:
    // "VERSION" label on the left, the dropdown on the right.
    const sidebar = document.querySelector(".wy-nav-side");
    if (sidebar) {
      const section = document.createElement("div");
      section.className = "version-section";

      const labelEl = document.createElement("span");
      labelEl.className = "version-label";
      labelEl.textContent = "Version";

      const wrapper = document.createElement("div");
      wrapper.className = "version-selector-wrapper";
      wrapper.appendChild(select);

      section.appendChild(labelEl);
      section.appendChild(wrapper);
      sidebar.appendChild(section);
    }
  }

  // Run only after the DOM is parsed, so the docs-base-path meta tag exists
  // even when this script is loaded earlier in <head> than the tag.
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();
