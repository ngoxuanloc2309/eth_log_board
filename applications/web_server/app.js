"use strict";

(() => {
  const POLL_INTERVAL_MS = 4000;
  const ERROR_RETRY_INTERVAL_MS = 10000;
  const REQUEST_TIMEOUT_MS = 7000;
  const STATUS_ENDPOINT = "/api/v1/status";
  const RADIO_CONFIG_ENDPOINT = "/api/v1/radio/config";
  const RADIO_READ_ENDPOINT = "/api/v1/radio/read";
  const SETTINGS_ENDPOINT = "/api/v1/settings";
  const FILESYSTEM_ENDPOINT = "/api/v1/files";
  const FILESYSTEM_DOWNLOAD_ENDPOINT = "/api/v1/files/download";
  const FILESYSTEM_MAX_PAGE_ENTRIES = 8;
  const FILESYSTEM_DOWNLOAD_CHUNK_SIZE = 1024;
  const FILESYSTEM_POLL_ATTEMPTS = 20;
  const FILESYSTEM_POLL_INTERVAL_MS = 400;

  let startupReady = false;
  const startDashboard = () => {
    if (startupReady) {
      return;
    }
    startupReady = true;
    updateFormState();
    void refreshDashboard();
    if (document.body.dataset.view === "filesystem") {
      void refreshFilesystem();
    }
    schedulePolling();
  };

  const elements = {
    connectionState: document.querySelector("#connection-state"),
    connectionLabel: document.querySelector("#connection-label"),
    pageTitle: document.querySelector("#page-title"),
    pageSubtitle: document.querySelector("#page-subtitle"),
    lastUpdated: document.querySelector("#last-updated"),
    globalNotice: document.querySelector("#global-notice"),
    refreshButton: document.querySelector("#refresh-button"),
    networkPill: document.querySelector("#network-pill"),
    networkAddress: document.querySelector("#network-address"),
    networkLink: document.querySelector("#network-link"),
    networkInterface: document.querySelector("#network-interface"),
    networkDhcp: document.querySelector("#network-dhcp"),
    networkNetmask: document.querySelector("#network-netmask"),
    networkGateway: document.querySelector("#network-gateway"),
    radioPill: document.querySelector("#radio-pill"),
    radioState: document.querySelector("#radio-state"),
    radioPanId: document.querySelector("#radio-pan-id"),
    radioChannel: document.querySelector("#radio-channel"),
    radioAddress: document.querySelector("#radio-address"),
    radioRole: document.querySelector("#radio-role"),
    radioOperation: document.querySelector("#radio-operation"),
    storagePill: document.querySelector("#storage-pill"),
    storageState: document.querySelector("#storage-state"),
    storageDescription: document.querySelector("#storage-description"),
    systemPill: document.querySelector("#system-pill"),
    systemCpu: document.querySelector("#system-cpu"),
    systemHeapUsed: document.querySelector("#system-heap-used"),
    systemHeapFree: document.querySelector("#system-heap-free"),
    systemHeapMinimum: document.querySelector("#system-heap-minimum"),
    systemTaskCount: document.querySelector("#system-task-count"),
    taskPill: document.querySelector("#task-pill"),
    taskDescription: document.querySelector("#task-description"),
    taskList: document.querySelector("#task-list"),
    configPill: document.querySelector("#config-pill"),
    configFields: document.querySelector("#config-fields"),
    configForm: document.querySelector("#config-form"),
    panIdInput: document.querySelector("#pan-id-input"),
    channelInput: document.querySelector("#channel-input"),
    addressInput: document.querySelector("#address-input"),
    roleInput: document.querySelector("#role-input"),
    transferInput: document.querySelector("#transfer-input"),
    baudInput: document.querySelector("#baud-input"),
    parityInput: document.querySelector("#parity-input"),
    antennaInput: document.querySelector("#antenna-input"),
    readConfigButton: document.querySelector("#read-config-button"),
    saveConfigButton: document.querySelector("#save-config-button"),
    operationMessage: document.querySelector("#operation-message"),
    operationState: document.querySelector("#operation-state"),
    operationDescription: document.querySelector("#operation-description"),
    activityIndicator: document.querySelector("#activity-indicator"),
    monitorView: document.querySelector("#monitor-view"),
    settingsView: document.querySelector("#settings-view"),
    filesystemView: document.querySelector("#filesystem-view"),
    viewLinks: document.querySelectorAll("[data-view-link]"),
    filesystemState: document.querySelector("#filesystem-state"),
    filesystemTotal: document.querySelector("#filesystem-total"),
    filesystemProgress: document.querySelector("#filesystem-progress"),
    filesystemUsed: document.querySelector("#filesystem-used"),
    filesystemFree: document.querySelector("#filesystem-free"),
    filesystemFileCount: document.querySelector("#filesystem-file-count"),
    filesystemDirectoryCount: document.querySelector("#filesystem-directory-count"),
    filesystemPath: document.querySelector("#filesystem-path"),
    filesystemMessage: document.querySelector("#filesystem-message"),
    filesystemList: document.querySelector("#filesystem-list"),
    filesystemBackButton: document.querySelector("#filesystem-back-button"),
    filesystemRefreshButton: document.querySelector("#filesystem-refresh-button"),
    filesystemPreviousButton: document.querySelector("#filesystem-previous-button"),
    filesystemNextButton: document.querySelector("#filesystem-next-button"),
    filesystemPageLabel: document.querySelector("#filesystem-page-label"),
    mqttSettingsForm: document.querySelector("#mqtt-settings-form"),
    mqttSettingsFields: document.querySelector("#mqtt-settings-fields"),
    mqttSettingsState: document.querySelector("#mqtt-settings-state"),
    mqttSettingsMessage: document.querySelector("#mqtt-settings-message"),
    mqttEnabled: document.querySelector("#mqtt-enabled"),
    mqttTls: document.querySelector("#mqtt-tls"),
    mqttHost: document.querySelector("#mqtt-host"),
    mqttPort: document.querySelector("#mqtt-port"),
    mqttClientId: document.querySelector("#mqtt-client-id"),
    mqttKeepAlive: document.querySelector("#mqtt-keep-alive"),
    ethernetSettingsForm: document.querySelector("#ethernet-settings-form"),
    ethernetSettingsFields: document.querySelector("#ethernet-settings-fields"),
    ethernetSettingsState: document.querySelector("#ethernet-settings-state"),
    ethernetSettingsMessage: document.querySelector("#ethernet-settings-message"),
    ethernetDhcp: document.querySelector("#ethernet-dhcp"),
    ethernetAddress: document.querySelector("#ethernet-address"),
    ethernetNetmask: document.querySelector("#ethernet-netmask"),
    ethernetGateway: document.querySelector("#ethernet-gateway"),
    confirmNetworkButton: document.querySelector("#confirm-network-button"),
    uartSettingsState: document.querySelector("#uart-settings-state"),
    uartSettingsForms: document.querySelectorAll(".uart-settings-form"),
    settingsSaveButtons: document.querySelectorAll(
      "#mqtt-settings-form button[type='submit'], " +
      "#ethernet-settings-form button[type='submit'], " +
      ".uart-settings-form button[type='submit']",
    ),
    productVariantState: document.querySelector("#product-variant-state"),
    productVariantValue: document.querySelector("#product-variant-value"),
  };

  let refreshInProgress = false;
  let actionInProgress = false;
  let refreshTimer = null;
  let radioInitialized = false;
  let radioBusy = false;
  let operationBusy = false;
  let configAvailable = false;
  let configurationDirty = false;
  let settingsLoaded = false;
  let settingsLoading = false;
  let liveNetworkDhcp = true;
  let loadedSettings = null;
  let apiRequestQueue = Promise.resolve();
  let lastOperationState = "idle";
  let lastOperationKind = "none";
  let filesystemCurrentPath = "/";
  let filesystemCurrentOffset = 0;
  let filesystemNextOffset = 0;
  let filesystemPageOffsets = [0];
  let filesystemRequestId = Date.now() >>> 0;
  let filesystemRefreshInProgress = false;
  let filesystemRefreshQueued = false;
  let filesystemDownloadInProgress = false;
  let filesystemHasMore = false;

  function showView(viewName) {
    const showSettings = viewName === "settings";
    const showFilesystem = viewName === "filesystem";
    document.body.dataset.view = viewName;
    elements.monitorView.hidden = showSettings || showFilesystem;
    elements.settingsView.hidden = !showSettings;
    elements.filesystemView.hidden = !showFilesystem;
    setText(
      elements.pageTitle,
      showSettings
        ? "Device settings"
        : showFilesystem
          ? "File system monitor"
          : "Device monitor",
    );
    setText(
      elements.pageSubtitle,
      showSettings
        ? "Configure device connectivity and serial settings."
        : showFilesystem
          ? "Review LittleFS capacity and browse directory metadata."
          : "Live health and radio configuration for your device.",
    );
    for (const link of elements.viewLinks) {
      if (link.dataset.viewLink === viewName) {
        link.setAttribute("aria-current", "page");
      } else {
        link.removeAttribute("aria-current");
      }
    }
  }

  function setText(element, value, fallback = "--") {
    element.textContent =
      value === null || value === undefined || value === "" ? fallback : String(value);
  }

  function setPill(element, label, kind = "neutral") {
    element.textContent = label;
    element.className = `pill pill-${kind}`;
  }

  function setMessage(element, message, kind = "info") {
    element.textContent = message;
    element.dataset.kind = kind;
    element.hidden = false;
  }

  function clearMessage(element) {
    element.textContent = "";
    element.hidden = true;
    delete element.dataset.kind;
  }

  function formatHex(value, digits = 4) {
    const number = typeof value === "string" ? Number(value) : value;
    if (!Number.isInteger(number) || number < 0 || number > 0xffff) {
      return "--";
    }
    return `0x${number.toString(16).toUpperCase().padStart(digits, "0")}`;
  }

  function displayChoice(value, choices) {
    return choices[value] || (value === undefined || value === null ? "--" : `Unknown (${value})`);
  }

  function setConnection(state, label) {
    elements.connectionState.dataset.state = state;
    setText(elements.connectionLabel, label);
  }

  function updateFormState() {
    const enabled =
      radioInitialized &&
      !radioBusy &&
      !operationBusy &&
      configAvailable &&
      !actionInProgress;
    elements.configFields.disabled = !enabled;
    elements.readConfigButton.disabled =
      !radioInitialized || radioBusy || operationBusy || actionInProgress;
    elements.saveConfigButton.disabled = !enabled;

    if (actionInProgress || radioBusy) {
      setPill(elements.configPill, "Operation running", "warning");
    } else if (enabled) {
      setPill(elements.configPill, "Device connected", "success");
    } else if (!radioInitialized) {
      setPill(elements.configPill, "Radio unavailable", "error");
    } else {
      setPill(elements.configPill, "Read only", "neutral");
    }
  }

  function updateNetwork(network) {
    const linkUp = network?.linkUp === true;
    const interfaceUp = network?.interfaceUp === true;
    const hasAddress = Boolean(network?.ip && network.ip !== "0.0.0.0");
    const networkReady = linkUp && interfaceUp && hasAddress;
    liveNetworkDhcp = network?.dhcp !== false;
    updateNetworkConfirmation();

    setPill(
      elements.networkPill,
      networkReady ? "Connected" : linkUp ? "No address" : "Disconnected",
      networkReady ? "success" : linkUp ? "warning" : "error",
    );
    setText(elements.networkAddress, hasAddress ? network.ip : "No address");
    setText(elements.networkLink, linkUp ? "Up" : "Down");
    setText(elements.networkInterface, interfaceUp ? "Up" : "Down");
    setText(elements.networkDhcp, network?.dhcp === true ? "Active" : "Inactive");
    setText(elements.networkNetmask, network?.netmask);
    setText(elements.networkGateway, network?.gateway);
  }

  function updateStorage(storage) {
    const mounted = storage?.mounted === true || storage?.available === true;
    const knownUnavailable =
      storage?.mounted === false || storage?.available === false;

    if (mounted) {
      setPill(elements.storagePill, "Available", "success");
      setText(elements.storageState, "Mounted");
      setText(elements.storageDescription, "LittleFS is mounted and ready for application use.");
    } else if (knownUnavailable) {
      setPill(elements.storagePill, "Unavailable", "warning");
      setText(elements.storageState, "Not mounted");
      setText(
        elements.storageDescription,
        "Storage is unavailable. The dashboard will not format flash.",
      );
    } else {
      setPill(elements.storagePill, "Unknown", "neutral");
      setText(elements.storageState, "Unknown");
      setText(elements.storageDescription, "Storage status was not provided by the device.");
    }
  }

  function formatBytes(bytes) {
    if (!Number.isSafeInteger(bytes) || bytes < 0) {
      return "--";
    }
    if (bytes < 1024) {
      return `${bytes} B`;
    }
    const units = ["KiB", "MiB", "GiB"];
    let value = bytes / 1024;
    let unitIndex = 0;
    while (value >= 1024 && unitIndex < units.length - 1) {
      value /= 1024;
      unitIndex++;
    }
    return `${value.toFixed(1)} ${units[unitIndex]}`;
  }

  function setFilesystemEmptyMessage(message) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.className = "empty-row";
    cell.colSpan = 4;
    cell.textContent = message;
    row.append(cell);
    elements.filesystemList.replaceChildren(row);
  }

  function updateFilesystemControls() {
    const disabled =
      filesystemRefreshInProgress || filesystemDownloadInProgress;
    elements.filesystemBackButton.disabled =
      disabled || filesystemCurrentPath === "/";
    elements.filesystemRefreshButton.disabled = disabled;
    elements.filesystemPreviousButton.disabled =
      disabled || filesystemCurrentOffset === 0;
    elements.filesystemNextButton.disabled = disabled || !filesystemHasMore;
    for (const button of elements.filesystemList.querySelectorAll(
      ".filesystem-download-button",
    )) {
      button.disabled = disabled;
    }
  }

  function filesystemParentPath(path) {
    const parentSeparator = path.lastIndexOf("/");
    return parentSeparator > 0 ? path.slice(0, parentSeparator) : "/";
  }

  function appendFilesystemEntry(entry) {
    const row = document.createElement("tr");
    const nameCell = document.createElement("td");
    const typeCell = document.createElement("td");
    const sizeCell = document.createElement("td");
    const actionCell = document.createElement("td");

    if (entry.isDirectory === true) {
      const link = document.createElement("button");
      link.className = "filesystem-entry-link";
      link.type = "button";
      link.textContent = entry.name;
      link.addEventListener("click", () => {
        requestFilesystemBrowse(
          `${filesystemCurrentPath === "/" ? "" : filesystemCurrentPath}/${entry.name}`,
          0,
        );
      });
      nameCell.append(link);
      typeCell.textContent = "Folder";
      actionCell.textContent = "--";
    } else {
      nameCell.textContent = entry.name;
      typeCell.textContent = "File";
      const downloadButton = document.createElement("button");
      downloadButton.className = "button button-secondary filesystem-download-button";
      downloadButton.type = "button";
      downloadButton.textContent = "Download";
      downloadButton.disabled =
        filesystemRefreshInProgress || filesystemDownloadInProgress;
      downloadButton.setAttribute("aria-label", `Download ${entry.name}`);
      downloadButton.addEventListener("click", () => {
        void downloadFilesystemFile(entry.name, entry.size);
      });
      actionCell.append(downloadButton);
    }
    sizeCell.textContent = entry.isDirectory === true ? "--" : formatBytes(entry.size);
    row.append(nameCell, typeCell, sizeCell, actionCell);
    elements.filesystemList.append(row);
  }

  function updateFilesystem(data) {
    const totalBytes = data?.totalBytes;
    const usedBytes = data?.usedBytes;
    const freeBytes = data?.freeBytes;
    const validCapacity =
      Number.isSafeInteger(totalBytes) &&
      Number.isSafeInteger(usedBytes) &&
      Number.isSafeInteger(freeBytes) &&
      totalBytes > 0 &&
      usedBytes >= 0 &&
      usedBytes <= totalBytes &&
      freeBytes === totalBytes - usedBytes;

    setPill(elements.filesystemState, "Mounted", "success");
    setText(elements.filesystemPath, data.path);
    setText(elements.filesystemTotal, formatBytes(totalBytes));
    setText(elements.filesystemUsed, formatBytes(usedBytes));
    setText(elements.filesystemFree, formatBytes(freeBytes));
    setText(elements.filesystemFileCount, data.fileCount);
    setText(elements.filesystemDirectoryCount, data.directoryCount);
    elements.filesystemProgress.value =
      validCapacity ? (usedBytes / totalBytes) * 100 : 0;
    elements.filesystemProgress.setAttribute(
      "aria-valuetext",
      validCapacity
        ? `${Math.round((usedBytes / totalBytes) * 100)} percent used`
        : "Usage unavailable",
    );

    elements.filesystemList.replaceChildren();
    if (!Array.isArray(data.entries) || data.entries.length === 0) {
      setFilesystemEmptyMessage("This directory is empty.");
    } else {
      for (const entry of data.entries) {
        appendFilesystemEntry(entry);
      }
    }
    filesystemNextOffset = data.nextOffset;
    filesystemHasMore = data.hasMore === true;
    updateFilesystemControls();
    const page = filesystemPageOffsets.length;
    setText(elements.filesystemPageLabel, `Page ${page}`);
    setText(
      elements.filesystemMessage,
      `${data.totalEntries} entries in this directory. Select Download to save a file.`,
    );
  }

  function filesystemRequestUrl(path, requestId, offset) {
    const query = new URLSearchParams({
      path,
      requestId: String(requestId),
      offset: String(offset),
    });
    return `${FILESYSTEM_ENDPOINT}?${query.toString()}`;
  }

  function filesystemDownloadRequestUrl(path, requestId, offset) {
    const query = new URLSearchParams({
      path,
      requestId: String(requestId),
      offset: String(offset),
    });
    return `${FILESYSTEM_DOWNLOAD_ENDPOINT}?${query.toString()}`;
  }

  async function requestFilesystemDownloadChunk(path, requestId, offset) {
    return runSerializedApiRequest(async () => {
      const response = await fetchWithTimeout(
        filesystemDownloadRequestUrl(path, requestId, offset),
        {
          method: "GET",
          headers: { Accept: "application/octet-stream, application/json" },
          cache: "no-store",
          credentials: "same-origin",
        },
      );

      if (response.status === 202) {
        const pending = await response.json();
        if (pending?.state !== "pending" || pending.requestId !== requestId) {
          throw new Error("The file download returned an invalid pending response.");
        }
        return { pending: true };
      }

      if (!response.ok) {
        let errorMessage = `Device request failed (${response.status}).`;
        try {
          const error = await response.json();
          errorMessage = error?.error || errorMessage;
        } catch {
          throw new Error(errorMessage);
        }
        throw new Error(errorMessage);
      }

      const totalSize = Number(response.headers.get("X-File-Size"));
      const responseOffset = Number(response.headers.get("X-File-Offset"));
      const responseRequestId = Number(response.headers.get("X-Request-Id"));
      const bytes = new Uint8Array(await response.arrayBuffer());
      if (
        !Number.isSafeInteger(totalSize) ||
        totalSize < 0 ||
        responseOffset !== offset ||
        responseRequestId !== requestId ||
        bytes.length > FILESYSTEM_DOWNLOAD_CHUNK_SIZE
      ) {
        throw new Error("The device returned invalid file data.");
      }
      return { pending: false, totalSize, bytes };
    });
  }

  async function downloadFilesystemFile(name, expectedSize) {
    if (filesystemDownloadInProgress || filesystemRefreshInProgress) {
      return;
    }

    filesystemDownloadInProgress = true;
    updateFilesystemControls();
    const path =
      `${filesystemCurrentPath === "/" ? "" : filesystemCurrentPath}/${name}`;
    const chunks = [];
    let offset = 0;
    let totalSize = null;
    setText(elements.filesystemMessage, `Preparing ${name} for download…`);

    try {
      do {
        filesystemRequestId = (filesystemRequestId + 1) >>> 0;
        if (filesystemRequestId === 0) {
          filesystemRequestId = 1;
        }
        const requestId = filesystemRequestId;
        let result;
        for (let attempt = 0; attempt < FILESYSTEM_POLL_ATTEMPTS; attempt++) {
          result = await requestFilesystemDownloadChunk(
            path,
            requestId,
            offset,
          );
          if (!result.pending) {
            break;
          }
          await new Promise((resolve) => {
            window.setTimeout(resolve, FILESYSTEM_POLL_INTERVAL_MS);
          });
        }
        if (!result || result.pending) {
          throw new Error("The file download is taking longer than expected.");
        }
        if (totalSize === null) {
          totalSize = result.totalSize;
          if (totalSize !== expectedSize) {
            throw new Error("The file changed after the directory was scanned.");
          }
        } else if (result.totalSize !== totalSize) {
          throw new Error("The file changed while it was being downloaded.");
        }

        const expectedChunkSize = Math.min(
          FILESYSTEM_DOWNLOAD_CHUNK_SIZE,
          totalSize - offset,
        );
        if (
          expectedChunkSize < 0 ||
          result.bytes.length !== expectedChunkSize
        ) {
          throw new Error("The device returned an incomplete file chunk.");
        }
        if (result.bytes.length > 0) {
          chunks.push(result.bytes);
          offset += result.bytes.length;
        } else if (totalSize !== 0) {
          throw new Error("The file download stopped before completion.");
        }
        setText(
          elements.filesystemMessage,
          `Downloading ${name}: ${formatBytes(offset)} of ${formatBytes(totalSize)}.`,
        );
      } while (offset < totalSize);

      const objectUrl = URL.createObjectURL(
        new Blob(chunks, { type: "application/octet-stream" }),
      );
      const downloadLink = document.createElement("a");
      downloadLink.href = objectUrl;
      downloadLink.download = name;
      document.body.append(downloadLink);
      downloadLink.click();
      downloadLink.remove();
      window.setTimeout(() => URL.revokeObjectURL(objectUrl), 1000);
      setText(elements.filesystemMessage, `Downloaded ${name}.`);
    } catch (error) {
      setText(elements.filesystemMessage, error.message);
    } finally {
      filesystemDownloadInProgress = false;
      updateFilesystemControls();
    }
  }

  function isValidFilesystemResponse(data, path, requestId, offset) {
    if (
      !data ||
      data.requestId !== requestId ||
      data.path !== path ||
      data.offset !== offset ||
      !Number.isSafeInteger(data.totalBytes) ||
      !Number.isSafeInteger(data.usedBytes) ||
      !Number.isSafeInteger(data.freeBytes) ||
      data.totalBytes <= 0 ||
      data.usedBytes < 0 ||
      data.usedBytes > data.totalBytes ||
      data.freeBytes !== data.totalBytes - data.usedBytes ||
      !Number.isSafeInteger(data.fileCount) ||
      !Number.isSafeInteger(data.directoryCount) ||
      !Number.isSafeInteger(data.totalEntries) ||
      data.fileCount < 0 ||
      data.directoryCount < 0 ||
      data.totalEntries !== data.fileCount + data.directoryCount ||
      !Array.isArray(data.entries) ||
      data.entries.length > FILESYSTEM_MAX_PAGE_ENTRIES ||
      typeof data.hasMore !== "boolean" ||
      !Number.isSafeInteger(data.nextOffset) ||
      data.nextOffset < offset ||
      data.nextOffset > data.totalEntries ||
      (data.hasMore && data.nextOffset === offset)
    ) {
      return false;
    }
    return data.entries.every(
      (entry) =>
        entry &&
        typeof entry.name === "string" &&
        entry.name.length > 0 &&
        !entry.name.includes("/") &&
        entry.name !== "." &&
        entry.name !== ".." &&
        typeof entry.isDirectory === "boolean" &&
        Number.isSafeInteger(entry.size) &&
        entry.size >= 0,
    );
  }

  function requestFilesystemBrowse(path, offset) {
    if (filesystemDownloadInProgress) {
      return;
    }
    if (path !== filesystemCurrentPath) {
      filesystemPageOffsets = [0];
    } else if (offset > filesystemCurrentOffset) {
      filesystemPageOffsets.push(offset);
    } else if (offset < filesystemCurrentOffset && filesystemPageOffsets.length > 1) {
      filesystemPageOffsets.pop();
    }
    filesystemCurrentPath = path;
    filesystemCurrentOffset = offset;
    if (filesystemRefreshInProgress) {
      filesystemRefreshQueued = true;
      return;
    }
    void refreshFilesystem();
  }

  async function refreshFilesystem() {
    if (filesystemRefreshInProgress || filesystemDownloadInProgress ||
        document.hidden) {
      return;
    }
    filesystemRefreshInProgress = true;
    updateFilesystemControls();
    setPill(elements.filesystemState, "Scanning", "neutral");
    setText(elements.filesystemPath, filesystemCurrentPath);
    setText(elements.filesystemMessage, "Reading LittleFS metadata…");
    setFilesystemEmptyMessage("Scanning directory.");

    const requestedPath = filesystemCurrentPath;
    const requestedOffset = filesystemCurrentOffset;
    filesystemRequestId = (filesystemRequestId + 1) >>> 0;
    if (filesystemRequestId === 0) {
      filesystemRequestId = 1;
    }
    const requestId = filesystemRequestId;

    try {
      let result;
      for (let attempt = 0; attempt < FILESYSTEM_POLL_ATTEMPTS; attempt++) {
        result = await getJson(
          filesystemRequestUrl(requestedPath, requestId, requestedOffset),
        );
        if (result?.state === "pending" && result.requestId !== requestId) {
          throw new Error("The file system returned a mismatched request.");
        }
        if (result?.state !== "pending") {
          break;
        }
        await new Promise((resolve) => {
          window.setTimeout(resolve, FILESYSTEM_POLL_INTERVAL_MS);
        });
      }
      if (!result || result.state === "pending") {
        throw new Error("The file system scan is taking longer than expected.");
      }
      if (!isValidFilesystemResponse(result, requestedPath, requestId,
                                     requestedOffset)) {
        throw new Error("The device returned invalid file system data.");
      }
      if (
        requestedPath === filesystemCurrentPath &&
        requestedOffset === filesystemCurrentOffset
      ) {
        updateFilesystem(result);
      }
    } catch (error) {
      if (
        error.message === "invalid_offset" &&
        requestedPath === filesystemCurrentPath &&
        requestedOffset !== 0
      ) {
        filesystemPageOffsets = [0];
        filesystemCurrentOffset = 0;
        filesystemRefreshQueued = true;
        return;
      }
      if (
        requestedPath === filesystemCurrentPath &&
        requestedOffset === filesystemCurrentOffset
      ) {
        setPill(elements.filesystemState, "Unavailable", "warning");
        setText(elements.filesystemTotal, "--");
        setText(elements.filesystemUsed, "--");
        setText(elements.filesystemFree, "--");
        setText(elements.filesystemFileCount, "--");
        setText(elements.filesystemDirectoryCount, "--");
        setFilesystemEmptyMessage("Directory information is unavailable.");
        setText(elements.filesystemMessage, error.message);
      }
    } finally {
      filesystemRefreshInProgress = false;
      updateFilesystemControls();
      if (filesystemRefreshQueued) {
        filesystemRefreshQueued = false;
        void refreshFilesystem();
      }
    }
  }

  function formatKibibytes(bytes) {
    if (!Number.isSafeInteger(bytes) || bytes < 0) {
      return "--";
    }
    return `${(bytes / 1024).toFixed(1)} KiB`;
  }

  function updateSystem(system) {
    if (!system || typeof system !== "object") {
      setPill(elements.systemPill, "Unavailable", "neutral");
      setText(elements.systemCpu, "Unavailable");
      setText(elements.systemHeapUsed, "--");
      setText(elements.systemHeapFree, "--");
      setText(elements.systemHeapMinimum, "--");
      setText(elements.systemTaskCount, "--");
      setPill(elements.taskPill, "Unavailable", "neutral");
      setText(elements.taskDescription, "System monitoring is not available in this firmware.");
      elements.taskList.replaceChildren();
      const row = document.createElement("tr");
      const cell = document.createElement("td");
      cell.className = "empty-row";
      cell.colSpan = 5;
      cell.textContent = "Task metrics are not available.";
      row.append(cell);
      elements.taskList.append(row);
      return;
    }

    const cpuValid =
      system.cpuAvailable === true &&
      Number.isInteger(system.cpuTenthsPercent) &&
      system.cpuTenthsPercent >= 0 &&
      system.cpuTenthsPercent <= 1000;
    const heapTotal = system.heapTotalBytes;
    const heapFree = system.heapFreeBytes;
    const heapMinimum = system.heapMinimumFreeBytes;
    const heapMetricsValid =
      Number.isSafeInteger(heapTotal) &&
      heapTotal > 0 &&
      Number.isSafeInteger(heapFree) &&
      heapFree >= 0 &&
      heapFree <= heapTotal;
    const heapUsed = heapMetricsValid ? heapTotal - heapFree : null;
    const heapUsedPercent = heapMetricsValid
      ? ((heapUsed / heapTotal) * 100).toFixed(1)
      : null;
    const taskCount = Number.isInteger(system.taskCount) ? system.taskCount : 0;
    const tasks = Array.isArray(system.tasks) ? system.tasks : [];

    setPill(elements.systemPill, cpuValid ? "Live" : "Sampling", cpuValid ? "success" : "neutral");
    setText(
      elements.systemCpu,
      cpuValid ? `${(system.cpuTenthsPercent / 10).toFixed(1)}%` : "Sampling…",
    );
    setText(
      elements.systemHeapUsed,
      heapMetricsValid && heapUsed !== null && heapUsedPercent !== null
        ? `${formatKibibytes(heapUsed)} / ${formatKibibytes(heapTotal)} (${heapUsedPercent}%)`
        : "--",
    );
    setText(
      elements.systemHeapFree,
      heapMetricsValid ? formatKibibytes(heapFree) : "--",
    );
    setText(
      elements.systemHeapMinimum,
      Number.isSafeInteger(heapMinimum) && heapMinimum >= 0
        ? formatKibibytes(heapMinimum)
        : "--",
    );
    setText(
      elements.systemTaskCount,
      `${taskCount}${system.tasksTruncated === true ? "+" : ""}`,
    );
    setPill(
      elements.taskPill,
      system.tasksTruncated === true ? `Showing ${tasks.length} of ${taskCount}` : `${tasks.length} tasks`,
      system.tasksTruncated === true ? "warning" : "success",
    );
    setText(
      elements.taskDescription,
      `Stack high-water values show the minimum remaining stack since each task started. CPU share is sampled over approximately one second.${system.tasksTruncated === true ? " The task list exceeds the fixed 16-task reporting limit." : ""}`,
    );

    elements.taskList.replaceChildren();
    if (tasks.length === 0) {
      const row = document.createElement("tr");
      const cell = document.createElement("td");
      cell.className = "empty-row";
      cell.colSpan = 5;
      cell.textContent = "No task metrics reported.";
      row.append(cell);
      elements.taskList.append(row);
      return;
    }

    for (const task of tasks) {
      const row = document.createElement("tr");
      const taskValues = [
        task?.name || "--",
        task?.state || "--",
        Number.isInteger(task?.priority) ? String(task.priority) : "--",
        formatKibibytes(task?.stackHighWaterBytes),
        cpuValid && Number.isInteger(task?.cpuTenthsPercent)
          ? `${(task.cpuTenthsPercent / 10).toFixed(1)}%`
          : "--",
      ];
      for (const value of taskValues) {
        const cell = document.createElement("td");
        cell.textContent = value;
        row.append(cell);
      }
      elements.taskList.append(row);
    }
  }

  function updateOperation(operation) {
    const state = operation?.state;
    const kind = operation?.kind;
    const error = operation?.error;
    const isPending = state === "pending" || state === "running";

    if (
      state === "success" &&
      (lastOperationState === "pending" || lastOperationState === "running") &&
      lastOperationKind === kind
    ) {
      configurationDirty = false;
    }
    if (state) {
      lastOperationState = state;
      lastOperationKind = kind;
    }
    operationBusy = isPending;

    elements.activityIndicator.dataset.state = isPending
      ? "pending"
      : state === "success"
        ? "success"
        : state === "failure" || state === "error"
          ? "error"
          : "idle";

    if (!state || state === "idle") {
      setText(elements.operationState, "No active operation");
      setText(elements.operationDescription, "Read or apply radio settings to start an operation.");
      setText(elements.radioOperation, "Idle");
      return;
    }

    const operationLabel = kind ? `${kind} · ${state}` : state;
    setText(elements.operationState, operationLabel);
    setText(elements.radioOperation, state);

    if (isPending) {
      setText(elements.operationDescription, "The device is processing the request.");
    } else if (state === "success") {
      setText(elements.operationDescription, "The operation completed successfully.");
    } else {
      setText(
        elements.operationDescription,
        error ? `The operation failed (error ${error}).` : "The operation failed. Check device availability and retry.",
      );
    }
  }

  function updateRadio(configResponse) {
    const config = configResponse?.config || configResponse?.parameters || configResponse;
    radioInitialized = configResponse?.initialized === true;
    radioBusy = configResponse?.busy === true;
    configAvailable =
      configResponse?.configAvailable === true &&
      radioInitialized &&
      config !== null &&
      typeof config === "object" &&
      (config.panId !== undefined || config.PAN_ID !== undefined);

    if (!radioInitialized) {
      setPill(elements.radioPill, "Unavailable", "error");
      setText(elements.radioState, "Not initialized");
      setText(elements.radioPanId, "--");
      setText(elements.radioChannel, "--");
      setText(elements.radioAddress, "--");
      setText(elements.radioRole, "--");
      updateFormState();
      return;
    }

    setPill(
      elements.radioPill,
      radioBusy ? "Busy" : "Ready",
      radioBusy ? "warning" : "success",
    );
    setText(elements.radioState, radioBusy ? "Operation in progress" : "Ready");

    if (!configAvailable) {
      setText(elements.radioPanId, "--");
      setText(elements.radioChannel, "--");
      setText(elements.radioAddress, "--");
      setText(elements.radioRole, "--");
      updateFormState();
      return;
    }

    const panId = config.panId ?? config.PAN_ID;
    const address = config.userAddress ?? config.user_address;
    const role = config.pointType ?? config.point_type;
    const channel = config.channel ?? config.Channel;
    const transferModel = config.transferModel ?? config.transfer_model;
    const baudRate = config.baudRate ?? config.uartBraudRate;
    const parity = config.parity ?? config.uartParity;
    const antenna = config.antennaSelect ?? config.antenna_select;

    setText(elements.radioPanId, formatHex(panId));
    setText(elements.radioChannel, channel);
    setText(elements.radioAddress, formatHex(address));
    setText(
      elements.radioRole,
      displayChoice(role, { 1: "Coordinator", 2: "Router", 3: "End device" }),
    );
    if (!configurationDirty) {
      elements.panIdInput.value = formatHex(panId);
      elements.channelInput.value = channel ?? "";
      elements.addressInput.value = formatHex(address);
      elements.roleInput.value = role ?? "";
      elements.transferInput.value = transferModel ?? "";
      elements.baudInput.value = baudRate ?? "";
      elements.parityInput.value = parity ?? "";
      elements.antennaInput.value = antenna ?? "";
    }
    updateFormState();
  }

  function runSerializedApiRequest(request) {
    const result = apiRequestQueue.then(request, request);
    apiRequestQueue = result.then(
      () => undefined,
      () => undefined,
    );
    return result;
  }

  function getJson(url) {
    return runSerializedApiRequest(async () => {
      const response = await fetchWithTimeout(url, {
        method: "GET",
        headers: { Accept: "application/json" },
        cache: "no-store",
        credentials: "same-origin",
      });
      const body = await response.text();
      let data = null;

      if (body) {
        try {
          data = JSON.parse(body);
        } catch {
          throw new Error(`The device returned invalid data for ${url}.`);
        }
      }
      if (!response.ok) {
        throw new Error(data?.message || data?.error || `Device request failed (${response.status}).`);
      }
      return data;
    });
  }

  function sendJson(url, method, payload) {
    return runSerializedApiRequest(async () => {
      const response = await fetchWithTimeout(url, {
        method,
        headers: {
          Accept: "application/json",
          "Content-Type": "application/json",
        },
        body: JSON.stringify(payload),
        cache: "no-store",
        credentials: "same-origin",
      });
      const body = await response.text();
      let data = null;

      if (body) {
        try {
          data = JSON.parse(body);
        } catch {
          throw new Error("The device returned an invalid response.");
        }
      }
      if (!response.ok) {
        throw new Error(data?.message || data?.error || `Request failed (${response.status}).`);
      }
      return data;
    });
  }

  function updateNetworkConfirmation() {
    const needsConfirmation =
      loadedSettings?.ethernet?.dhcpEnabled === false && !liveNetworkDhcp;
    elements.confirmNetworkButton.hidden = !needsConfirmation;
    if (needsConfirmation) {
      setPill(elements.ethernetSettingsState, "Confirm static network", "warning");
    }
  }

  function updateSaveState(state) {
    if (state === "pending") {
      setSettingsSaveButtonsDisabled(true);
      setPill(elements.mqttSettingsState, "Saving", "warning");
      setPill(elements.ethernetSettingsState, "Saving", "warning");
      setPill(elements.uartSettingsState, "Saving", "warning");
    } else if (state === "saved") {
      setSettingsSaveButtonsDisabled(false);
      setPill(elements.mqttSettingsState, "Saved · reboot required", "success");
      setPill(elements.ethernetSettingsState, "Saved · reboot required", "success");
      setPill(elements.uartSettingsState, "Saved · reboot required", "success");
      updateNetworkConfirmation();
    } else if (state === "failed") {
      setSettingsSaveButtonsDisabled(false);
      setPill(elements.mqttSettingsState, "Save failed", "error");
      setPill(elements.ethernetSettingsState, "Save failed", "error");
      setPill(elements.uartSettingsState, "Save failed", "error");
    } else {
      setSettingsSaveButtonsDisabled(false);
    }
  }

  function setSettingsSaveButtonsDisabled(disabled) {
    for (const button of elements.settingsSaveButtons) {
      button.disabled = disabled;
    }
    elements.confirmNetworkButton.disabled = disabled;
  }

  function setUartFormValues(form, config) {
    for (const name of ["baudRate", "dataBits", "parity", "stopBits"]) {
      form.elements.namedItem(name).value = String(config[name]);
    }
    form.querySelector("fieldset").disabled = false;
  }

  async function loadSettings() {
    if (settingsLoading || settingsLoaded) {
      return;
    }
    settingsLoading = true;
    try {
      const settings = await getJson(SETTINGS_ENDPOINT);
      loadedSettings = settings;

      elements.mqttEnabled.checked = settings.mqtt.enabled === true;
      elements.mqttTls.checked = settings.mqtt.tlsEnabled === true;
      elements.mqttHost.value = settings.mqtt.host;
      elements.mqttPort.value = String(settings.mqtt.port);
      elements.mqttClientId.value = settings.mqtt.clientId;
      elements.mqttKeepAlive.value = String(settings.mqtt.keepAliveSeconds);
      elements.mqttSettingsFields.disabled = false;
      setPill(elements.mqttSettingsState, "Ready", "success");

      elements.ethernetDhcp.checked = settings.ethernet.dhcpEnabled === true;
      elements.ethernetAddress.value = settings.ethernet.address;
      elements.ethernetNetmask.value = settings.ethernet.netmask;
      elements.ethernetGateway.value = settings.ethernet.gateway;
      elements.ethernetSettingsFields.disabled = false;
      updateEthernetFields();
      setPill(elements.ethernetSettingsState, "Ready", "success");

      for (const form of elements.uartSettingsForms) {
        const port = form.dataset.uartPort;
        if (!Object.prototype.hasOwnProperty.call(settings.uart, port)) {
          throw new Error(`The device did not return settings for ${port}.`);
        }
        setUartFormValues(form, settings.uart[port]);
      }
      setPill(elements.uartSettingsState, "Ready", "success");

      const hasVariant = settings.productVariant !== null &&
        settings.productVariant !== undefined &&
        settings.productVariant !== "";
      setText(
        elements.productVariantValue,
        hasVariant ? settings.productVariant : "Not provisioned",
      );
      setPill(
        elements.productVariantState,
        hasVariant ? "Read only" : "Unprovisioned",
        hasVariant ? "neutral" : "warning",
      );
      settingsLoaded = true;
      updateSaveState(settings.saveState);
      updateNetworkConfirmation();
    } catch (error) {
      setPill(elements.mqttSettingsState, "Unavailable", "error");
      setPill(elements.ethernetSettingsState, "Unavailable", "error");
      setPill(elements.uartSettingsState, "Unavailable", "error");
      setMessage(elements.mqttSettingsMessage, error.message, "error");
      setMessage(elements.ethernetSettingsMessage, error.message, "error");
      for (const form of elements.uartSettingsForms) {
        setMessage(form.querySelector(".uart-settings-message"), error.message, "error");
      }
    } finally {
      settingsLoading = false;
    }
  }

  async function refreshSettingsSaveState() {
    try {
      const settings = await getJson(SETTINGS_ENDPOINT);
      if (loadedSettings) {
        loadedSettings.saveState = settings.saveState;
      }
      updateSaveState(settings.saveState);
    } catch (error) {
      setMessage(elements.mqttSettingsMessage, error.message, "error");
    }
  }

  function updateEthernetFields() {
    const staticFieldsDisabled = elements.ethernetDhcp.checked;
    elements.ethernetAddress.disabled = staticFieldsDisabled;
    elements.ethernetNetmask.disabled = staticFieldsDisabled;
    elements.ethernetGateway.disabled = staticFieldsDisabled;
  }

  async function saveSettingsForm(
    url,
    payload,
    messageElement,
    successText,
    applyLocalSettings,
  ) {
    try {
      await sendJson(url, "PUT", payload);
      setMessage(messageElement, successText, "success");
      updateSaveState("pending");
      if (loadedSettings) {
        loadedSettings.saveState = "pending";
        applyLocalSettings(loadedSettings);
        updateNetworkConfirmation();
      }
    } catch (error) {
      setMessage(messageElement, error.message, "error");
    }
  }

  async function fetchWithTimeout(url, options) {
    const controller = new AbortController();
    const timeout = window.setTimeout(
      () => controller.abort(),
      REQUEST_TIMEOUT_MS,
    );

    try {
      return await fetch(url, { ...options, signal: controller.signal });
    } catch (error) {
      if (error.name === "AbortError") {
        throw new Error("The device request timed out.");
      }
      throw error;
    } finally {
      window.clearTimeout(timeout);
    }
  }

  async function refreshDashboard() {
    if (!startupReady || refreshInProgress || document.hidden) {
      return;
    }
    refreshInProgress = true;
    elements.refreshButton.disabled = true;

    try {
      const status = await getJson(STATUS_ENDPOINT);
      setConnection("online", "Device online");
      clearMessage(elements.globalNotice);
      schedulePolling(POLL_INTERVAL_MS);
      updateNetwork(status?.network);
      updateStorage(status?.storage);
      updateSystem(status?.system);
      updateOperation(status?.operation);

      try {
        const radio = await getJson(RADIO_CONFIG_ENDPOINT);
        updateRadio(radio);
      } catch (error) {
        radioInitialized = false;
        radioBusy = false;
        configAvailable = false;
        setPill(elements.radioPill, "Unavailable", "warning");
        setText(elements.radioState, "Status unavailable");
        setText(elements.radioPanId, "--");
        setText(elements.radioChannel, "--");
        setText(elements.radioAddress, "--");
        setText(elements.radioRole, "--");
        updateFormState();
        setMessage(elements.globalNotice, error.message, "info");
      }

      if (!settingsLoaded) {
        await loadSettings();
      } else if (loadedSettings?.saveState === "pending") {
        await refreshSettingsSaveState();
      }

      elements.lastUpdated.dateTime = new Date().toISOString();
      setText(
        elements.lastUpdated,
        new Intl.DateTimeFormat(undefined, {
          hour: "2-digit",
          minute: "2-digit",
          second: "2-digit",
        }).format(new Date()),
      );
    } catch (error) {
      setConnection("offline", "Device unavailable");
      setPill(elements.networkPill, "Unavailable", "error");
      setText(elements.networkAddress, "No connection");
      setText(elements.networkLink, "--");
      setText(elements.networkInterface, "--");
      setText(elements.networkDhcp, "--");
      setText(elements.networkNetmask, "--");
      setText(elements.networkGateway, "--");
      radioInitialized = false;
      radioBusy = false;
      configAvailable = false;
      updateFormState();
      setMessage(
        elements.globalNotice,
        `${error.message} Status will be retried automatically.`,
        "error",
      );
      schedulePolling(ERROR_RETRY_INTERVAL_MS);
    } finally {
      refreshInProgress = false;
      elements.refreshButton.disabled = false;
    }
  }

  function parseHexField(input, label) {
    const text = input.value.trim();
    if (!/^(?:0x)?[0-9a-fA-F]{1,4}$/.test(text)) {
      throw new Error(`${label} must be a hexadecimal value such as 0x2B02.`);
    }
    const value = Number.parseInt(text.replace(/^0x/i, ""), 16);
    if (value < 1 || value > 0xff00) {
      throw new Error(`${label} must be between 0x0001 and 0xFF00.`);
    }
    return value;
  }

  function readConfigurationForm() {
    const channel = Number(elements.channelInput.value);
    if (!Number.isInteger(channel) || channel < 11 || channel > 26) {
      throw new Error("Channel must be between 11 and 26.");
    }

    return {
      panId: parseHexField(elements.panIdInput, "PAN ID"),
      channel,
      userAddress: parseHexField(elements.addressInput, "Device address"),
      pointType: Number(elements.roleInput.value),
      transferModel: Number(elements.transferInput.value),
      baudRate: Number(elements.baudInput.value),
      dataBits: 8,
      stopBits: 1,
      parity: Number(elements.parityInput.value),
      antennaSelect: Number(elements.antennaInput.value),
    };
  }

  async function performAction(action, successMessage, operationKind) {
    actionInProgress = true;
    updateFormState();
    clearMessage(elements.operationMessage);

    try {
      const response = await action();
      setMessage(elements.operationMessage, successMessage, "info");
      if (response?.operation) {
        updateOperation(response.operation);
      } else {
        updateOperation({
          kind: operationKind,
          state: "pending",
        });
      }
    } catch (error) {
      setMessage(elements.operationMessage, error.message, "error");
    } finally {
      actionInProgress = false;
      updateFormState();
    }
  }

  elements.refreshButton.addEventListener("click", () => {
    void refreshDashboard();
  });

  elements.filesystemRefreshButton.addEventListener("click", () => {
    void refreshFilesystem();
  });
  elements.filesystemBackButton.addEventListener("click", () => {
    requestFilesystemBrowse(filesystemParentPath(filesystemCurrentPath), 0);
  });
  elements.filesystemPreviousButton.addEventListener("click", () => {
    const previousOffset =
      filesystemPageOffsets.length > 1
        ? filesystemPageOffsets[filesystemPageOffsets.length - 2]
        : 0;
    requestFilesystemBrowse(filesystemCurrentPath, previousOffset);
  });
  elements.filesystemNextButton.addEventListener("click", () => {
    requestFilesystemBrowse(filesystemCurrentPath, filesystemNextOffset);
  });

  function updateViewFromLocation() {
    const viewName =
      window.location.hash === "#settings"
        ? "settings"
        : window.location.hash === "#filesystem"
          ? "filesystem"
          : "monitor";
    showView(viewName);
    if (viewName === "settings" && startupReady) {
      void loadSettings();
    }
    if (viewName === "filesystem" && startupReady) {
      void refreshFilesystem();
    }
    if (window.location.hash !== `#${viewName}`) {
      window.history.replaceState(null, "", `#${viewName}`);
    }
  }

  window.addEventListener("hashchange", updateViewFromLocation);
  updateViewFromLocation();

  elements.configForm.addEventListener("input", () => {
    configurationDirty = true;
  });
  elements.configForm.addEventListener("change", () => {
    configurationDirty = true;
  });

  elements.readConfigButton.addEventListener("click", () => {
    void performAction(
      () => sendJson(RADIO_READ_ENDPOINT, "POST", {}),
      "Radio read requested.",
      "radio-read",
    );
  });

  elements.configForm.addEventListener("submit", (event) => {
    event.preventDefault();
    if (!elements.configForm.reportValidity()) {
      return;
    }

    let configuration;
    try {
      configuration = readConfigurationForm();
    } catch (error) {
      setMessage(elements.operationMessage, error.message, "error");
      return;
    }

    if (!window.confirm("Apply these settings to the radio module?")) {
      return;
    }

    void performAction(
      () => sendJson(RADIO_CONFIG_ENDPOINT, "PUT", configuration),
      "Configuration update requested.",
      "radio-update",
    );
  });

  elements.mqttSettingsForm.addEventListener("input", () => {
    clearMessage(elements.mqttSettingsMessage);
  });
  elements.mqttSettingsForm.addEventListener("submit", (event) => {
    event.preventDefault();
    if (!elements.mqttSettingsForm.reportValidity()) {
      return;
    }

    const config = {
      enabled: elements.mqttEnabled.checked,
      tlsEnabled: elements.mqttTls.checked,
      host: elements.mqttHost.value.trim(),
      port: Number(elements.mqttPort.value),
      clientId: elements.mqttClientId.value.trim(),
      keepAliveSeconds: Number(elements.mqttKeepAlive.value),
    };
    void saveSettingsForm(
      `${SETTINGS_ENDPOINT}/mqtt`,
      config,
      elements.mqttSettingsMessage,
      "MQTT settings queued. They will take effect after reboot.",
      (settings) => {
        settings.mqtt = config;
      },
    );
  });

  elements.ethernetDhcp.addEventListener("change", () => {
    updateEthernetFields();
    clearMessage(elements.ethernetSettingsMessage);
  });
  elements.ethernetSettingsForm.addEventListener("input", () => {
    clearMessage(elements.ethernetSettingsMessage);
  });
  elements.ethernetSettingsForm.addEventListener("submit", (event) => {
    event.preventDefault();
    if (!elements.ethernetSettingsForm.reportValidity()) {
      return;
    }
    const config = {
      dhcpEnabled: elements.ethernetDhcp.checked,
      address: elements.ethernetAddress.value.trim(),
      netmask: elements.ethernetNetmask.value.trim(),
      gateway: elements.ethernetGateway.value.trim(),
    };
    if (!config.dhcpEnabled &&
        !window.confirm("Save this static network configuration? You must confirm connectivity after reboot within 60 seconds or the device will restore DHCP.")) {
      return;
    }
    void saveSettingsForm(
      `${SETTINGS_ENDPOINT}/ethernet`,
      config,
      elements.ethernetSettingsMessage,
      "Ethernet settings queued. They will take effect after reboot.",
      (settings) => {
        settings.ethernet = config;
      },
    );
  });

  for (const form of elements.uartSettingsForms) {
    form.addEventListener("input", () => {
      clearMessage(form.querySelector(".uart-settings-message"));
    });
    form.addEventListener("submit", (event) => {
      event.preventDefault();
      if (!form.reportValidity()) {
        return;
      }
      const port = form.dataset.uartPort;
      const config = {
        baudRate: Number(form.elements.namedItem("baudRate").value),
        dataBits: Number(form.elements.namedItem("dataBits").value),
        parity: Number(form.elements.namedItem("parity").value),
        stopBits: Number(form.elements.namedItem("stopBits").value),
      };
      void saveSettingsForm(
        `${SETTINGS_ENDPOINT}/${port}`,
        config,
        form.querySelector(".uart-settings-message"),
        `${port.toUpperCase()} settings queued. They will take effect after reboot.`,
        (settings) => {
          settings.uart[port] = config;
        },
      );
    });
  }

  elements.confirmNetworkButton.addEventListener("click", async () => {
    if (!window.confirm("Confirm that the static network configuration is reachable?")) {
      return;
    }
    try {
      await sendJson(`${SETTINGS_ENDPOINT}/network/confirm`, "POST", {});
      elements.confirmNetworkButton.hidden = true;
      setPill(elements.ethernetSettingsState, "Network confirmed", "success");
      setMessage(
        elements.ethernetSettingsMessage,
        "Static network confirmed for this boot.",
        "success",
      );
    } catch (error) {
      setMessage(elements.ethernetSettingsMessage, error.message, "error");
    }
  });

  function schedulePolling(interval = POLL_INTERVAL_MS) {
    if (!startupReady || document.hidden) {
      return;
    }
    if (refreshTimer !== null) {
      window.clearInterval(refreshTimer);
    }
    refreshTimer = window.setInterval(() => {
      void refreshDashboard();
    }, interval);
  }

  document.addEventListener("visibilitychange", () => {
    if (document.hidden) {
      if (refreshTimer !== null) {
        window.clearInterval(refreshTimer);
        refreshTimer = null;
      }
      return;
    }
    void refreshDashboard();
    schedulePolling();
  });

  startDashboard();
})();
