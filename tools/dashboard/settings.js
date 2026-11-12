"use strict";
// Runtime-only telemetry and guarded controls. Use a separate local authoring instance to edit flows.
module.exports = {
    uiHost: "127.0.0.1",
    uiPort: 1880,
    httpAdminRoot: false,
    httpStatic: [{path: require('node:path').join(__dirname, 'public'), root: '/'}],
    flowFile: "flows.json",
    credentialSecret: process.env.ROBOCAR_CREDENTIAL_SECRET,
    functionGlobalContext: { robocar: require("./decode"), control: require("./control").hub },
    editorTheme: { projects: { enabled: false } },
    logging: { console: { level: "info", metrics: false, audit: false } }
};
