import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Controls.Material

import org.streetpea.chiaking

import "controls" as C

// NLFork: remap any controller (Xbox, generic, pro pads with back buttons) to the PS5 buttons.
// Each PS5 action can use one button or a combo of two; the touchpad press is one of them.
DialogView {
    id: dialog
    title: qsTr("Controller")
    buttonVisible: false

    property var entries: Chiaki.controllerRemapList()
    property string captureAction: ""
    property string captureName: ""
    property int captureIndex: -1
    property int secondsLeft: 0

    function refresh(focusIndex) {
        entries = Chiaki.controllerRemapList();
        if (focusIndex >= 0)
            Qt.callLater(() => {
                const item = rows.itemAt(focusIndex);
                if (item)
                    item.bindingButton.forceActiveFocus(Qt.TabFocusReason);
            });
    }

    function startCapture(index) {
        const entry = entries[index];
        captureAction = entry.action;
        captureName = entry.name;
        captureIndex = index;
        secondsLeft = 8;
        captureTimer.restart();
        captureOverlay.forceActiveFocus();
        Chiaki.startButtonCapture();
    }

    function stopCapture() {
        captureTimer.stop();
        Chiaki.cancelButtonCapture();
        const index = captureIndex;
        captureAction = "";
        refresh(index);
    }

    Component.onDestruction: {
        if (captureAction !== "")
            Chiaki.cancelButtonCapture();
    }

    // DialogView's default property only takes one Item, so helpers live inside it
    Item {
        Connections {
            target: Chiaki
            function onButtonCaptured(binding) {
                if (dialog.captureAction === "")
                    return;
                captureTimer.stop();
                const index = dialog.captureIndex;
                const notice = Chiaki.setControllerRemapBinding(dialog.captureAction, binding);
                dialog.captureAction = "";
                dialog.refresh(index);
                if (notice !== "") {
                    noticeLabel.text = notice;
                    noticeTimer.restart();
                }
            }
            function onControllerRemapChanged() {
                if (dialog.captureAction === "")
                    dialog.entries = Chiaki.controllerRemapList();
            }
        }

        Timer {
            id: noticeTimer
            interval: 6000
        }

        // Tells the user when another PS5 button had to give up the button they just picked
        Rectangle {
            z: 5
            anchors {
                horizontalCenter: parent.horizontalCenter
                bottom: parent.bottom
                bottomMargin: 18
            }
            width: noticeLabel.implicitWidth + 48
            height: noticeLabel.implicitHeight + 26
            radius: height / 2
            color: "#3a2a0a"
            border.color: "#ffb340"
            opacity: noticeTimer.running ? 1 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 200 } }

            Label {
                id: noticeLabel
                anchors.centerIn: parent
                color: "#ffd28a"
                font.pixelSize: 17
                font.bold: true
            }
        }

        Timer {
            id: captureTimer
            interval: 1000
            repeat: true
            onTriggered: {
                dialog.secondsLeft--;
                if (dialog.secondsLeft <= 0)
                    dialog.stopCapture();
            }
        }

        ScrollView {
            id: scroll
            anchors {
                fill: parent
                topMargin: 14
                bottomMargin: 10
            }
            contentWidth: availableWidth
            clip: true

            ColumnLayout {
                width: scroll.availableWidth
                spacing: 14

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Pick a PS5 button, then press the button or two-button combo you want for it")
                    font.pixelSize: 18
                }

                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 20

                    C.Button {
                        id: resetButton
                        firstInFocusChain: true
                        text: qsTr("Reset to default")
                        onClicked: root.showConfirmDialog(qsTr("Reset controller"),
                                                          qsTr("Go back to the default button layout?"),
                                                          () => { Chiaki.resetControllerRemap(); dialog.refresh(-1); })
                    }

                    Label {
                        text: qsTr("Square (or X on Xbox) on a row removes its button")
                        opacity: 0.7
                    }
                }

                GridLayout {
                    Layout.alignment: Qt.AlignHCenter
                    columns: 2
                    columnSpacing: 60
                    rowSpacing: 6

                    Repeater {
                        id: rows
                        model: dialog.entries

                        RowLayout {
                            property alias bindingButton: bindingButton
                            spacing: 16

                            Label {
                                Layout.preferredWidth: 220
                                horizontalAlignment: Text.AlignRight
                                text: modelData.name
                                font.pixelSize: 17
                                font.bold: modelData.action === "touchpad"
                                color: modelData.action === "touchpad" ? Material.accent : Material.foreground
                            }

                            C.Button {
                                id: bindingButton
                                Layout.preferredWidth: 280
                                lastInFocusChain: index === dialog.entries.length - 1
                                text: (modelData.available ? "" : "⚠ ") + modelData.label + (modelData.custom ? "  ●" : "")
                                font.capitalization: Font.MixedCase
                                Material.background: activeFocus ? Material.accent : (modelData.available ? "#16233f" : "#3a2a0a")
                                Material.foreground: modelData.binding === "" ? "#8a93a6" : (modelData.available ? "#ffffff" : "#ffb340")
                                ToolTip.visible: hovered && !modelData.available
                                ToolTip.text: qsTr("Your controller doesn't have this button: pick another one")
                                onClicked: dialog.startCapture(index)
                                Keys.onPressed: (event) => {
                                    if (event.key === Qt.Key_No || event.key === Qt.Key_Delete || event.key === Qt.Key_Backspace) {
                                        Chiaki.setControllerRemapBinding(modelData.action, "");
                                        dialog.refresh(index);
                                        event.accepted = true;
                                    }
                                }
                            }
                        }
                    }
                }

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 900
                    Layout.topMargin: 6
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    opacity: 0.75
                    text: qsTr("Tip: for games that ask you to press the touchpad, give Touchpad a button you rarely use (View, a back paddle) or a combo such as View + Menu. Back paddles show up as P1–P4 when the controller reports them; if your controller copies another button with its paddles, set that up on the controller first. ● = changed from the default.")
                }
            }
        }

        // Shown while waiting for the new button
        Rectangle {
            id: captureOverlay
            anchors.fill: parent
            visible: dialog.captureAction !== ""
            color: Qt.rgba(0.02, 0.04, 0.09, 0.92)
            focus: visible

            Keys.onEscapePressed: dialog.stopCapture()
            Keys.onPressed: (event) => event.accepted = true

            MouseArea {
                anchors.fill: parent
                onClicked: dialog.stopCapture()
            }

            ColumnLayout {
                anchors.centerIn: parent
                spacing: 18

                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: dialog.captureName
                    font.pixelSize: 34
                    font.bold: true
                    color: Material.accent
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Press the button, or hold two buttons together for a combo")
                    font.pixelSize: 22
                }
                Label {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("It is saved when you let go · Esc or click to cancel (%1 s)").arg(dialog.secondsLeft)
                    opacity: 0.7
                }
            }
        }
    }
}
