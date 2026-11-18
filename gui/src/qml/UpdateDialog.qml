import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Shapes

import org.streetpea.chiaking

import "controls" as C

// NLFork: PS5-style update window. Shows what's new, then download -> verify -> install -> restart.
Item {
    id: updateDialog
    property bool open: false
    property Item restoreFocusItem
    readonly property var updater: Chiaki.updater
    readonly property int st: updater ? updater.state : 0
    readonly property bool busy: st === NLForkUpdater.Downloading || st === NLForkUpdater.Verifying
                                 || st === NLForkUpdater.Installing || st === NLForkUpdater.Restarting

    function show() {
        restoreFocusItem = Window.window.activeFocusItem;
        open = true;
        Qt.callLater(focusDefault);
    }

    function hide() {
        if (busy)
            return;
        open = false;
        if (restoreFocusItem && restoreFocusItem.visible)
            restoreFocusItem.forceActiveFocus(Qt.TabFocusReason);
    }

    function focusDefault() {
        if (st === NLForkUpdater.Available)
            installButton.forceActiveFocus(Qt.TabFocusReason);
        else if (st === NLForkUpdater.Error)
            retryButton.forceActiveFocus(Qt.TabFocusReason);
        else if (busy)
            cancelButton.forceActiveFocus(Qt.TabFocusReason);
        else
            okButton.forceActiveFocus(Qt.TabFocusReason);
    }

    function formatMB(bytes) {
        return (bytes / (1024 * 1024)).toFixed(bytes > 100 * 1024 * 1024 ? 0 : 1);
    }

    onStChanged: if (open) Qt.callLater(focusDefault)

    visible: opacity > 0
    opacity: open ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    // Dim everything behind and swallow clicks
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.01, 0.02, 0.05, 0.86)
        MouseArea { anchors.fill: parent }
    }

    Keys.onEscapePressed: hide()

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 980)
        height: Math.min(parent.height - 80, 620)
        radius: 26
        color: "#0e1630"
        border.color: Qt.rgba(1, 1, 1, 0.10)
        scale: updateDialog.open ? 1 : 0.94
        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#13204a" }
            GradientStop { position: 1.0; color: "#0a1024" }
        }

        // Thin accent line along the top, like the PS5 system cards
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right; leftMargin: 28; rightMargin: 28; topMargin: 0 }
            height: 4
            radius: 2
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#00d4ff" }
                GradientStop { position: 1.0; color: "#8a5cff" }
            }
        }

        // ------------------------------------------------------------ what's new
        ColumnLayout {
            anchors { fill: parent; margins: 44 }
            spacing: 18
            visible: !updateDialog.busy

            RowLayout {
                spacing: 22
                Layout.fillWidth: true

                // Download glyph in a glowing ring
                Item {
                    Layout.preferredWidth: 86
                    Layout.preferredHeight: 86
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "#00b4ff" }
                            GradientStop { position: 1.0; color: "#7a4cff" }
                        }
                        SequentialAnimation on scale {
                            running: updateDialog.open && st === NLForkUpdater.Available
                            loops: Animation.Infinite
                            NumberAnimation { from: 1; to: 1.06; duration: 900; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 1.06; to: 1; duration: 900; easing.type: Easing.InOutSine }
                        }
                    }
                    Label {
                        anchors.centerIn: parent
                        text: st === NLForkUpdater.Error ? "!" : (st === NLForkUpdater.UpToDate ? "✓" : "↓")
                        font.pixelSize: 44
                        font.bold: true
                        color: "white"
                    }
                }

                ColumnLayout {
                    spacing: 4
                    Layout.fillWidth: true
                    Label {
                        text: {
                            if (st === NLForkUpdater.Checking) return qsTr("Checking for updates…");
                            if (st === NLForkUpdater.UpToDate) return qsTr("NLFork is up to date");
                            if (st === NLForkUpdater.Error) return qsTr("The update didn't work");
                            return qsTr("Update available");
                        }
                        font.pixelSize: 34
                        font.bold: true
                        color: "white"
                    }
                    Label {
                        text: {
                            if (!updater) return "";
                            if (st === NLForkUpdater.UpToDate || st === NLForkUpdater.Checking)
                                return qsTr("Version %1").arg(updater.currentVersion);
                            if (st === NLForkUpdater.Error)
                                return updater.errorString;
                            return qsTr("NLFork %1 · you have %2").arg(updater.latestVersion).arg(updater.currentVersion);
                        }
                        font.pixelSize: 19
                        color: st === NLForkUpdater.Error ? "#ffb340" : "#9fb4d9"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // Release notes from GitHub
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 16
                color: Qt.rgba(1, 1, 1, 0.04)
                border.color: Qt.rgba(1, 1, 1, 0.06)
                visible: st === NLForkUpdater.Available || (st === NLForkUpdater.Error && updater && updater.releaseNotes !== "")

                ScrollView {
                    id: notesScroll
                    anchors { fill: parent; margins: 20 }
                    clip: true
                    Label {
                        width: notesScroll.availableWidth
                        text: updater ? updater.releaseNotes : ""
                        textFormat: Text.MarkdownText
                        wrapMode: Text.WordWrap
                        font.pixelSize: 17
                        color: "#dbe6ff"
                        onLinkActivated: (link) => Qt.openUrlExternally(link)
                    }
                }
            }

            Item { Layout.fillHeight: true; visible: !(st === NLForkUpdater.Available || st === NLForkUpdater.Error) }

            RowLayout {
                spacing: 14
                Layout.alignment: Qt.AlignRight

                C.Button {
                    id: githubButton
                    firstInFocusChain: true
                    lastInFocusChain: true
                    KeyNavigation.right: st === NLForkUpdater.Available ? laterButton : retryButton
                    text: qsTr("View on GitHub")
                    flat: true
                    visible: st === NLForkUpdater.Available || st === NLForkUpdater.Error
                    onClicked: updater.openReleasePage()
                }
                C.Button {
                    id: laterButton
                    firstInFocusChain: true
                    lastInFocusChain: true
                    KeyNavigation.left: githubButton
                    KeyNavigation.right: installButton
                    text: qsTr("Later")
                    flat: true
                    visible: st === NLForkUpdater.Available
                    onClicked: updateDialog.hide()
                }
                C.Button {
                    id: retryButton
                    firstInFocusChain: true
                    lastInFocusChain: true
                    KeyNavigation.left: githubButton
                    KeyNavigation.right: okButton
                    text: qsTr("Try again")
                    visible: st === NLForkUpdater.Error
                    onClicked: updater.latestVersion !== "" && updater.updateAvailable ? updater.installUpdate() : updater.checkForUpdates()
                }
                C.Button {
                    id: okButton
                    firstInFocusChain: true
                    lastInFocusChain: true
                    KeyNavigation.left: retryButton.visible ? retryButton : okButton
                    text: qsTr("Close")
                    visible: st !== NLForkUpdater.Available
                    onClicked: { if (st === NLForkUpdater.Error) updater.cancel(); updateDialog.hide(); }
                }
                C.Button {
                    id: installButton
                    firstInFocusChain: true
                    lastInFocusChain: true
                    KeyNavigation.left: laterButton
                    text: qsTr("Install now")
                    visible: st === NLForkUpdater.Available
                    font.pixelSize: 19
                    font.bold: true
                    Material.background: activeFocus ? "#00b4ff" : "#1673d1"
                    Material.foreground: "white"
                    onClicked: updater.installUpdate()
                }
            }
        }

        // ------------------------------------------------------------ installing
        Item {
            anchors { fill: parent; margins: 44 }
            visible: updateDialog.busy

            Label {
                anchors { top: parent.top; horizontalCenter: parent.horizontalCenter }
                text: updater ? qsTr("Updating to NLFork %1").arg(updater.latestVersion) : ""
                font.pixelSize: 26
                font.bold: true
                color: "white"
            }

            // Progress ring with a soft glow; spins while there is no exact progress
            Item {
                id: ring
                width: 230
                height: 230
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -20
                readonly property real value: st === NLForkUpdater.Downloading ? (updater ? updater.progress : 0) : 1
                readonly property bool indeterminate: st !== NLForkUpdater.Downloading

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width + 40
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.width: 18
                    border.color: Qt.rgba(0, 0.7, 1, 0.08)
                }

                Shape {
                    anchors.fill: parent
                    layer.enabled: true
                    layer.samples: 8
                    ShapePath {
                        strokeColor: Qt.rgba(1, 1, 1, 0.08)
                        strokeWidth: 12
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc { centerX: 115; centerY: 115; radiusX: 105; radiusY: 105; startAngle: 0; sweepAngle: 360 }
                    }
                }

                Shape {
                    id: arcShape
                    anchors.fill: parent
                    layer.enabled: true
                    layer.samples: 8
                    rotation: 0
                    ShapePath {
                        strokeColor: "#19c3ff"
                        strokeWidth: 12
                        fillColor: "transparent"
                        capStyle: ShapePath.RoundCap
                        PathAngleArc {
                            centerX: 115; centerY: 115; radiusX: 105; radiusY: 105
                            startAngle: -90
                            sweepAngle: ring.indeterminate ? 90 : Math.max(2, 360 * ring.value)
                            Behavior on sweepAngle { NumberAnimation { duration: 250 } }
                        }
                    }
                    RotationAnimation on rotation {
                        running: ring.indeterminate && updateDialog.visible
                        from: 0; to: 360
                        duration: 1100
                        loops: Animation.Infinite
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: st === NLForkUpdater.Downloading ? Math.round((updater ? updater.progress : 0) * 100) + "%"
                          : st === NLForkUpdater.Restarting ? "↻" : "…"
                    font.pixelSize: st === NLForkUpdater.Downloading ? 52 : 60
                    font.bold: true
                    color: "white"
                }
            }

            Label {
                id: statusLabel
                anchors { horizontalCenter: parent.horizontalCenter; top: ring.bottom; topMargin: 34 }
                text: {
                    if (!updater) return "";
                    if (st === NLForkUpdater.Downloading)
                        return qsTr("Downloading… %1 MB of %2 MB").arg(formatMB(updater.receivedBytes)).arg(formatMB(updater.totalBytes));
                    if (st === NLForkUpdater.Verifying) return qsTr("Checking the download…");
                    if (st === NLForkUpdater.Installing) return qsTr("Installing…");
                    return qsTr("Restarting NLFork…");
                }
                font.pixelSize: 21
                color: "#cfe0ff"
            }

            // Step dots: download, verify, install, restart
            Row {
                anchors { horizontalCenter: parent.horizontalCenter; top: statusLabel.bottom; topMargin: 22 }
                spacing: 34
                Repeater {
                    model: [qsTr("Download"), qsTr("Verify"), qsTr("Install"), qsTr("Restart")]
                    Column {
                        id: stepCol
                        spacing: 8
                        readonly property int stepIndex: st === NLForkUpdater.Downloading ? 0 : st === NLForkUpdater.Verifying ? 1 : st === NLForkUpdater.Installing ? 2 : 3
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 14; height: 14; radius: 7
                            color: index < parent.stepIndex ? "#22d47b" : index === parent.stepIndex ? "#19c3ff" : Qt.rgba(1, 1, 1, 0.18)
                            SequentialAnimation on opacity {
                                running: index === stepCol.stepIndex && updateDialog.visible
                                loops: Animation.Infinite
                                NumberAnimation { from: 1; to: 0.35; duration: 600 }
                                NumberAnimation { from: 0.35; to: 1; duration: 600 }
                            }
                        }
                        Label {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: modelData
                            font.pixelSize: 15
                            color: index <= parent.stepIndex ? "#dbe6ff" : "#6f7d99"
                        }
                    }
                }
            }

            C.Button {
                id: cancelButton
                firstInFocusChain: true
                lastInFocusChain: true
                anchors { right: parent.right; bottom: parent.bottom }
                text: qsTr("Cancel")
                flat: true
                visible: st === NLForkUpdater.Downloading
                onClicked: updater.cancel()
            }

            Label {
                anchors { left: parent.left; bottom: parent.bottom }
                text: qsTr("Don't turn off the PC. NLFork will open again by itself.")
                font.pixelSize: 15
                color: "#7f8fb0"
            }
        }
    }
}
