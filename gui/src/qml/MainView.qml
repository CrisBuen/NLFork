import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

// NLFork home screen: console-style layout. Consoles are large tiles in a horizontal row, the
// selected one is enlarged, and its details and actions are shown below it. Everything is plain
// Qt Quick (no shader effects), so it stays light on the GPU.
Pane {
    padding: 0
    id: consolePane

    readonly property int tileSize: Math.round(Math.max(200, Math.min(380, height * 0.30)))
    readonly property var currentHost: hostsView.currentItem && hostsView.currentItem.visible ? hostsView.currentItem : null
    readonly property int visibleHostCount: {
        let n = 0;
        for (let i = 0; i < Chiaki.hosts.length; i++)
            if (Chiaki.hosts[i].display)
                n++;
        return n;
    }

    function moveHost(step) {
        if (!hostsView.currentItem)
            return;
        for (let k = 0; k < hostsView.count; k++) {
            if (step > 0)
                hostsView.incrementCurrentIndex();
            else
                hostsView.decrementCurrentIndex();
            if (hostsView.currentItem.visible)
                break;
        }
    }

    background: Image {
        source: "qrc:/icons/home-bg.svg"
        sourceSize: Qt.size(Math.max(1, width), Math.max(1, height))
        fillMode: Image.Stretch
        cache: true
    }

    StackView.onActivated: {
        forceActiveFocus(Qt.TabFocusReason);
        if(!Chiaki.autoConnect && !root.initialAsk && !Chiaki.window.directStream)
        {
            root.initialAsk = true;
            if(Chiaki.settings.addSteamShortcutAsk && (typeof Chiaki.createSteamShortcut === "function"))
                root.showRemindDialog(qsTr("Official Steam artwork + controller layout"), qsTr("Would you like to either create a new non-Steam game for NLFork\nor update an existing non-Steam game with the official artwork and controller layout?") + "\n\n" + qsTr("(Note: If you select no now and want to do this later, click the button or press R3 from the main menu.)"), false, () => root.showSteamShortcutDialog(true));
            else if(Chiaki.settings.remotePlayAsk)
            {
                if(!Chiaki.settings.psnRefreshToken || !Chiaki.settings.psnAuthToken || !Chiaki.settings.psnAuthTokenExpiry || !Chiaki.settings.psnAccountId)
                    root.showRemindDialog(qsTr("Remote Play via PSN"), qsTr("Would you like to connect to PSN?\nThis enables:\n- Automatic registration\n- Playing outside of your home network without port forwarding?") + "\n\n" + qsTr("(Note: If you select no now and want to do this later, go to the Config section of the settings.)"), true, () => root.showPSNTokenDialog(false));
                else
                    Chiaki.settings.remotePlayAsk = false;
            }
        }
    }
    // The tiles run left to right; up/down also move, as in the old vertical list
    Keys.onLeftPressed: moveHost(-1)
    Keys.onRightPressed: moveHost(1)
    Keys.onUpPressed: moveHost(-1)
    Keys.onDownPressed: moveHost(1)
    Keys.onMenuPressed: settingsButton.clicked()
    Keys.onReturnPressed: if (hostsView.currentItem) hostsView.currentItem.connectToHost()
    Keys.onYesPressed: if (hostsView.currentItem) hostsView.currentItem.wakeUpHost()
    Keys.onNoPressed: if (hostsView.currentItem) hostsView.currentItem.deleteHost()
    Keys.onEscapePressed: root.showConfirmDialog(qsTr("Quit"), qsTr("Are you sure you want to quit?"), () => Qt.quit())
    Keys.onPressed: (event) => {
        if (event.modifiers)
            return;
        switch (event.key) {
        case Qt.Key_PageUp:
            if (hostsView.currentItem) hostsView.currentItem.setConsolePin();
            event.accepted = true;
            break;
        case Qt.Key_PageDown:
            if (Chiaki.settings.psnAuthToken) Chiaki.refreshPsnToken();
            event.accepted = true;
            break;
        case Qt.Key_F1:
            if (typeof Chiaki.createSteamShortcut === "function") root.showSteamShortcutDialog(false);
            event.accepted = true;
            break;
        case Qt.Key_F2:
            root.showManualHostDialog();
            event.accepted = true;
            break;
        }
    }

    // Rounded, flat button used across the home screen
    component PillButton: Button {
        flat: !highlighted
        focusPolicy: Qt.NoFocus
        leftPadding: 22
        rightPadding: 22
        Material.roundedScale: Material.FullScale
        font.pixelSize: 18
    }

    // Controller hint shown inside a button (e.g. the triangle for Wake Up)
    component HintIcon: Image {
        anchors {
            left: parent.left
            verticalCenter: parent.verticalCenter
            leftMargin: 14
        }
        width: 26
        height: 26
        sourceSize: Qt.size(width, height)
    }

    // ---------------------------------------------------------------- top bar
    Item {
        id: topBar
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: 96

        RowLayout {
            anchors {
                fill: parent
                leftMargin: 40
                rightMargin: 28
            }
            spacing: 14

            Image {
                Layout.preferredWidth: 46
                Layout.preferredHeight: 46
                source: "qrc:/icons/nlfork.svg"
                sourceSize: Qt.size(width, height)
            }

            Image {
                Layout.preferredHeight: 30
                Layout.preferredWidth: 30 * 500 / 160
                source: "qrc:/icons/nlfork-logo-white.svg"
                sourceSize: Qt.size(width, height)
            }

            Item { Layout.preferredWidth: 30 }

            // One-click stream profiles. Keep the values in sync with QmlSettings::applyStreamProfile().
            // A button is highlighted while the current settings still match its profile.
            PillButton {
                highlighted: Chiaki.settings.videoPreset == 3
                    && Chiaki.settings.customShaders.length == 0
                    && Chiaki.settings.bitrateLocalPS5 == 40000
                text: qsTr("Competitive")
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Lowest input lag: FSRCNNX 8, no extra shader, 40 Mbps")
                onClicked: {
                    Chiaki.settings.applyStreamProfile(1);
                    Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQualitySpatial;
                }
            }

            PillButton {
                highlighted: Chiaki.settings.videoPreset == 4
                    && Chiaki.settings.customShaders.length == 2
                    && Chiaki.settings.customShaders[0] == ":/shaders/CAS-suave.glsl"
                    && Chiaki.settings.customShaders[1] == ":/shaders/color-nitido.glsl"
                    && Chiaki.settings.bitrateLocalPS5 == 100000
                text: qsTr("Quality")
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Best picture: FSRCNNX 16 + soft CAS sharpening + sharp color, 100 Mbps")
                onClicked: {
                    Chiaki.settings.applyStreamProfile(2);
                    Chiaki.window.videoPreset = ChiakiWindow.VideoPreset.HighQualityAdvancedSpatial;
                }
            }

            Item { Layout.fillWidth: true }

            PillButton {
                text: qsTr("Steam Shortcut")
                visible: typeof Chiaki.createSteamShortcut === "function"
                onClicked: root.showSteamShortcutDialog(false)
            }

            PillButton {
                text: qsTr("Refresh PSN Hosts")
                visible: Chiaki.settings.psnAuthToken
                onClicked: Chiaki.refreshPsnToken()
            }

            PillButton {
                text: qsTr("Add Console")
                icon.source: "qrc:/icons/add-24px.svg"
                icon.width: 22
                icon.height: 22
                onClicked: root.showManualHostDialog()
            }

            Label {
                id: clock
                Layout.leftMargin: 10
                Layout.rightMargin: 6
                font.pixelSize: 22
                opacity: 0.9
                text: Qt.formatTime(new Date(), "HH:mm")
                Timer {
                    interval: 15000
                    running: true
                    repeat: true
                    onTriggered: clock.text = Qt.formatTime(new Date(), "HH:mm")
                }
            }

            RoundButton {
                id: settingsButton
                flat: true
                icon.source: "qrc:/icons/settings-20px.svg"
                icon.width: 34
                icon.height: 34
                padding: 12
                focusPolicy: Qt.NoFocus
                onClicked: root.showSettingsDialog()
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Settings")
            }

            RoundButton {
                flat: true
                text: "×"
                font.pixelSize: 34
                padding: 6
                focusPolicy: Qt.NoFocus
                onClicked: Qt.quit()
                ToolTip.visible: hovered
                ToolTip.delay: 500
                ToolTip.text: qsTr("Quit")
            }
        }
    }

    // ---------------------------------------------------------------- console tiles
    Label {
        id: rowTitle
        anchors {
            top: topBar.bottom
            left: parent.left
            topMargin: Math.round(consolePane.height * 0.06)
            leftMargin: 80
        }
        text: qsTr("Consoles")
        font.pixelSize: 22
        opacity: 0.75
        visible: consolePane.visibleHostCount > 0
    }

    ListView {
        id: hostsView
        orientation: ListView.Horizontal
        keyNavigationWraps: true
        anchors {
            top: rowTitle.bottom
            left: parent.left
            right: parent.right
            topMargin: 18
        }
        height: consolePane.tileSize + 30
        leftMargin: 80
        rightMargin: 80
        spacing: 26
        clip: false
        interactive: contentWidth > width
        highlightMoveDuration: 180
        preferredHighlightBegin: 80
        preferredHighlightEnd: width - 80
        highlightRangeMode: ListView.ApplyRange
        model: Chiaki.hosts
        onCountChanged: {
            if(!hostsView.currentItem)
                hostsView.incrementCurrentIndex();
            if(!hostsView.currentItem)
                return;
            if(!hostsView.currentItem.visible)
            {
                for(var i = 0; i < hostsView.count; i++)
                {
                    hostsView.incrementCurrentIndex()
                    if(hostsView.currentItem.visible)
                    {
                        break;
                    }
                }
            }
        }
        delegate: Item {
            id: delegate
            visible: modelData.display
            width: modelData.display ? consolePane.tileSize : 0
            height: consolePane.tileSize
            readonly property var host: modelData
            readonly property int hostIndex: index
            readonly property bool highlighted: ListView.isCurrentItem

            function connectToHost() {
                if(modelData.discovered)
                    Chiaki.connectToHost(index, modelData.name);
                else
                    Chiaki.connectToHost(index);
            }

            function wakeUpHost() {
                if(!modelData.discovered && !modelData.duid)
                    Chiaki.wakeUpHost(index);
            }

            function deleteHost() {
                if (modelData.manual)
                    root.showConfirmDialog(qsTr("Delete Console"), qsTr("Are you sure you want to delete this console?"), () => {Chiaki.deleteHost(index)});
                else if (modelData.discovered && !modelData.registered)
                    root.showConfirmDialog(qsTr("Hide Console"), qsTr("Are you sure you want to hide this console?") + "\n\n" + qsTr("Note: You can unhide from the Consoles section of the Settings under Hidden Consoles"), () => Chiaki.hideHost(modelData.mac, modelData.name));
            }

            function setConsolePin() {
                root.showConsolePinDialog(index);
            }

            Rectangle {
                id: tile
                anchors.centerIn: parent
                width: consolePane.tileSize
                height: consolePane.tileSize
                radius: 24
                scale: delegate.highlighted ? 1.0 : 0.84
                opacity: delegate.highlighted ? 1.0 : 0.75
                gradient: Gradient {
                    GradientStop { position: 0.0; color: delegate.highlighted ? "#25345f" : "#18213d" }
                    GradientStop { position: 1.0; color: delegate.highlighted ? "#141c36" : "#0f1529" }
                }
                border.width: delegate.highlighted ? 3 : 1
                border.color: delegate.highlighted ? Material.accent : "#2a3558"

                Behavior on scale { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 150 } }

                Image {
                    anchors {
                        horizontalCenter: parent.horizontalCenter
                        verticalCenter: parent.verticalCenter
                        verticalCenterOffset: -parent.height * 0.08
                    }
                    width: parent.width * 0.62
                    height: parent.height * 0.42
                    fillMode: Image.PreserveAspectFit
                    source: "image://svg/console-ps" + (modelData.ps5 ? "5" : "4") + (modelData.state == "standby" ? "#light_standby" : "#light_on")
                    sourceSize: Qt.size(width, height)
                }

                // State light: ready, rest mode, or unknown / remote
                Rectangle {
                    anchors {
                        top: parent.top
                        right: parent.right
                        margins: 18
                    }
                    width: 14
                    height: 14
                    radius: 7
                    color: modelData.state == "ready" ? "#3ddc84" : (modelData.state == "standby" ? "#ffa726" : "#6b7699")
                }

                Label {
                    anchors {
                        left: parent.left
                        right: parent.right
                        bottom: parent.bottom
                        margins: 18
                    }
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                    font.pixelSize: 20
                    font.bold: delegate.highlighted
                    text: modelData.name ? modelData.name : (modelData.ps5 ? "PS5" : "PS4")
                }
            }

            MouseArea {
                anchors.fill: parent
                // First click selects, a click on the selected tile starts the stream
                onClicked: {
                    if (hostsView.currentIndex === index)
                        delegate.connectToHost();
                    else
                        hostsView.currentIndex = index;
                }
            }
        }
    }

    // ---------------------------------------------------------------- selected console
    ColumnLayout {
        id: details
        anchors {
            top: hostsView.bottom
            left: parent.left
            right: parent.right
            topMargin: 34
            leftMargin: 80
            rightMargin: 80
        }
        spacing: 10
        visible: consolePane.currentHost !== null
        readonly property var h: consolePane.currentHost ? consolePane.currentHost.host : null

        Label {
            text: details.h ? (details.h.name ? details.h.name : (details.h.ps5 ? "PS5" : "PS4")) : ""
            font.pixelSize: 44
            font.bold: true
        }

        Label {
            opacity: 0.8
            font.pixelSize: 19
            text: {
                const h = details.h;
                if (!h)
                    return "";
                let parts = [];
                if (!h.duid) {
                    if (h.state == "ready")
                        parts.push(qsTr("Ready"));
                    else if (h.state == "standby")
                        parts.push(qsTr("Rest mode"));
                    else if (h.state)
                        parts.push(h.state);
                }
                if (h.duid)
                    parts.push(h.discovered ? qsTr("Automatic Registration Available") : qsTr("Remote Connection via PSN"));
                else if (h.discovered)
                    parts.push(h.manual ? qsTr("discovered + manual") : qsTr("discovered"));
                else
                    parts.push(qsTr("manual"));
                if (h.mac)
                    parts.push(h.registered ? qsTr("registered") : qsTr("unregistered"));
                if (h.address)
                    parts.push(Chiaki.settings.streamerMode ? qsTr("address hidden") : h.address);
                return parts.join("   ·   ");
            }
        }

        Label {
            opacity: 0.65
            font.pixelSize: 17
            visible: text.length > 0
            text: {
                const h = details.h;
                if (!h || h.duid || !h.discovered)
                    return "";
                let parts = [];
                if (h.app)
                    parts.push(qsTr("App: %1").arg(h.app));
                if (h.titleId)
                    parts.push(qsTr("Title ID: %1").arg(h.titleId));
                if (h.mac && !Chiaki.settings.streamerMode)
                    parts.push(qsTr("ID: %1").arg(h.mac));
                return parts.join("   ·   ");
            }
        }

        RowLayout {
            Layout.topMargin: 18
            spacing: 14

            PillButton {
                highlighted: true
                text: qsTr("Start")
                font.pixelSize: 22
                font.bold: true
                topPadding: 14
                bottomPadding: 14
                leftPadding: 44
                rightPadding: 44
                onClicked: if (consolePane.currentHost) consolePane.currentHost.connectToHost()
            }

            PillButton {
                text: qsTr("Wake Up")
                leftPadding: 50
                visible: details.h !== null && details.h.registered && !details.h.duid && !details.h.discovered
                onClicked: if (consolePane.currentHost) consolePane.currentHost.wakeUpHost()
                HintIcon { source: root.controllerButton("pyramid") }
            }

            PillButton {
                text: qsTr("Update Console Pin")
                leftPadding: 50
                visible: details.h !== null && details.h.registered
                onClicked: if (consolePane.currentHost) consolePane.currentHost.setConsolePin()
                HintIcon { source: "qrc:/icons/l1.svg" }
            }

            PillButton {
                text: details.h && details.h.manual ? qsTr("Delete") : qsTr("Hide")
                leftPadding: 50
                visible: details.h !== null && (details.h.manual || (details.h.discovered && !details.h.registered))
                onClicked: if (consolePane.currentHost) consolePane.currentHost.deleteHost()
                HintIcon { source: root.controllerButton("box") }
            }
        }
    }

    // ---------------------------------------------------------------- empty state
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 24
        visible: consolePane.visibleHostCount === 0

        Image {
            id: logoImage
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/icons/nlfork-logo-white.svg"
            // The wordmark is wide (500x160): size it by width
            sourceSize.width: Math.min(consolePane.width * 0.45, consolePane.height * 0.9)
            opacity: 0.18
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            font.pixelSize: 22
            opacity: 0.7
            text: Chiaki.discoveryEnabled ? qsTr("Searching for consoles on your network…")
                                          : qsTr("Console discovery is off. Turn it on below or add a console.")
        }
    }

    // ---------------------------------------------------------------- bottom
    RoundButton {
        anchors {
            left: parent.left
            bottom: parent.bottom
            margins: 24
        }
        icon.source: "qrc:/icons/discover-" + (checked ? "" : "off-") + "24px.svg"
        icon.width: 40
        icon.height: 40
        padding: 16
        focusPolicy: Qt.NoFocus
        checkable: true
        checked: Chiaki.discoveryEnabled
        onToggled: Chiaki.discoveryEnabled = !Chiaki.discoveryEnabled
        Material.background: Material.accent
        ToolTip.visible: hovered
        ToolTip.delay: 500
        ToolTip.text: checked ? qsTr("Console discovery on") : qsTr("Console discovery off")
    }

    Label {
        anchors {
            right: parent.right
            bottom: parent.bottom
            margins: 24
        }
        opacity: 0.5
        text: "NLFork " + Qt.application.version
    }
}
