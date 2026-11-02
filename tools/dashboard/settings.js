"use strict";
// Runtime-only receiver. Use a separate local authoring instance to edit flows.
module.exports = {
    uiHost: "127.0.0.1",
    uiPort: 1880,
    httpAdminRoot: false,
    flowFile: "flows.json",
    credentialSecret: process.env.ROBOCAR_CREDENTIAL_SECRET,
    functionGlobalContext: { robocar: require("./decode") },
    editorTheme: { projects: { enabled: false } },
    logging: { console: { level: "info", metrics: false, audit: false } }
};
