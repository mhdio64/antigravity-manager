import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    function label(english, persian) { return releases.persian ? persian : english }
    LayoutMirroring.enabled: releases.persian
    LayoutMirroring.childrenInherit: true
    width: 820
    minimumWidth: 420
    height: 690
    minimumHeight: 400
    visible: true
    title: label("Antigravity Release Check", "مدیر آنتی‌گراویتی")

    Dialog {
        id: confirmMigration
        property int productIndex: -1
        anchors.centerIn: parent
        modal: true
        title: label("Set up a managed installation?", "نصب مدیریت‌شده ایجاد شود؟")
        standardButtons: Dialog.Ok | Dialog.Cancel
        onAccepted: releases.install(productIndex)
        Label {
            width: 400
            wrapMode: Text.Wrap
            text: label("Close the existing product first. The manager will install a fresh copy and keep your settings and existing files.",
                        "ابتدا برنامهٔ فعلی را ببندید. مدیر نسخه‌ای تازه نصب می‌کند و تنظیمات و فایل‌های فعلی را نگه می‌دارد.")
        }
    }

    FileDialog {
        id: reportFile
        title: label("Save support report", "ذخیرهٔ گزارش پشتیبانی")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Text files (*.txt)"]
        onAccepted: reportStatus.text = releases.exportReport(selectedFile)
            ? label("Report saved.", "گزارش ذخیره شد.")
            : label("Could not save report.", "ذخیرهٔ گزارش ممکن نبود.")
    }

    Dialog {
        id: confirmUpdate
        property int productIndex: -1
        anchors.centerIn: parent
        modal: true
        title: label("Update Antigravity?", "آنتی‌گراویتی به‌روزرسانی شود؟")
        footer: DialogButtonBox {
            Button {
                text: label("Cancel", "انصراف")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: label("Update", "به‌روزرسانی")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            onAccepted: confirmUpdate.accept()
            onRejected: confirmUpdate.reject()
        }
        onAccepted: releases.install(productIndex)
        Label {
            text: label("The updated version starts with RTL off.\nYou can enable it again once compatibility is verified.",
                        "نسخهٔ جدید با راست‌چین غیرفعال شروع می‌شود.\nپس از تأیید سازگاری می‌توانید دوباره آن را فعال کنید.")
        }
    }

    Dialog {
        id: confirmRemoval
        property int productIndex: -1
        anchors.centerIn: parent
        title: productIndex === 0 ? label("Remove Antigravity App?", "برنامهٔ آنتی‌گراویتی حذف شود؟") : label("Remove Antigravity IDE?", "محیط توسعهٔ آنتی‌گراویتی حذف شود؟")
        modal: true
        footer: DialogButtonBox {
            Button {
                text: label("Cancel", "انصراف")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: label("Remove", "حذف")
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
            onAccepted: confirmRemoval.accept()
            onRejected: confirmRemoval.reject()
        }
        onAccepted: releases.remove(productIndex)
        Label {
            text: label("This removes the installed and previous versions.\nYour settings and projects are kept.", "نسخهٔ نصب‌شده و نسخهٔ قبلی حذف می‌شوند.\nتنظیمات و پروژه‌ها حفظ می‌شوند.")
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: availableWidth
        clip: true

        Column {
            width: parent.width
            spacing: 16

            Label {
                text: label("Your applications", "برنامه‌های شما")
                font.pixelSize: 30
                font.weight: Font.DemiBold
            }

            Flow {
                width: parent.width
                spacing: 10
                Button {
                    text: label("Check for updates", "بررسی به‌روزرسانی")
                    enabled: !releases.busy
                    onClicked: releases.check()
                }
                ComboBox {
                    model: ["English", "فارسی"]
                    currentIndex: releases.persian ? 1 : 0
                    enabled: !releases.busy
                    Accessible.name: label("Interface language", "زبان رابط")
                    onActivated: releases.persian = currentIndex === 1
                }
            }

            Repeater {
                model: releases.results
                delegate: Frame {
                    id: card
                    required property string modelData
                    required property int index
                    property bool expanded: false
                    readonly property bool isInstalled: { releases.results; return releases.installed(index) }
                    readonly property string version: { releases.results; return releases.localVersion(index) }
                    readonly property bool updateAvailable: {
                        card.modelData
                        return card.isInstalled && card.version.length > 0 && releases.canUpdate(index)
                    }
                    readonly property bool current: {
                        card.modelData
                        return card.isInstalled && card.version.length > 0
                            && releases.available(index) && !releases.canUpdate(index)
                    }
                    width: parent.width
                    padding: 20
                    background: Rectangle {
                        radius: 14
                        color: systemColors.base
                        border.color: card.updateAvailable ? systemColors.highlight : systemColors.mid
                    }

                    contentItem: Column {
                        spacing: 12
                        Label {
                            width: parent.width
                            text: card.index === 0 ? "Antigravity App" : "Antigravity IDE"
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                        }
                        Label {
                            width: parent.width
                            wrapMode: Text.Wrap
                            color: systemColors.windowText
                            text: card.isInstalled
                                ? (card.version.length ? label("Installed · ", "نصب‌شده · ") + "\u2066" + card.version + "\u2069"
                                                       : label("Installed · version unknown", "نصب‌شده · نسخه نامشخص"))
                                : label("Not installed", "نصب نشده")
                        }
                        Rectangle {
                            width: Math.min(parent.width, releaseStatus.implicitWidth + 20)
                            height: releaseStatus.implicitHeight + 10
                            radius: 7
                            color: card.updateAvailable ? systemColors.highlight
                                : card.current ? Qt.tint(systemColors.base,
                                    Qt.rgba(systemColors.highlight.r, systemColors.highlight.g,
                                            systemColors.highlight.b, 0.14))
                                : systemColors.base
                            Label {
                                id: releaseStatus
                                width: parent.width - 20
                                anchors.centerIn: parent
                                wrapMode: Text.Wrap
                                color: card.updateAvailable ? systemColors.highlightedText : systemColors.text
                                font.weight: card.current || card.updateAvailable ? Font.DemiBold : Font.Normal
                                text: card.updateAvailable ? label("Update available", "به‌روزرسانی موجود است")
                                    : card.current ? label("Up to date", "به‌روز است")
                                    : card.modelData.split("\n")[0]
                            }
                        }
                    Label {
                        property string detected: { releases.results; return releases.externalInstall(index) }
                        visible: detected.length > 0
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        text: label("Existing installation found: ", "نصب قبلی پیدا شد: ") + "\u2066" + detected + "\u2069"
                    }
                        Flow {
                            width: parent.width
                            spacing: 8
                        Button {
                            text: label("Launch", "اجرا")
                            highlighted: !card.updateAvailable
                            visible: { releases.results; return releases.installed(index) }
                            enabled: !releases.busy
                            onClicked: releases.launch(index)
                        }
                        Button {
                            text: { releases.results; return releases.installed(index) ? label("Update", "به‌روزرسانی") : label("Install", "نصب") }
                            highlighted: !card.isInstalled || card.updateAvailable
                            enabled: { releases.results; return !releases.busy && releases.canUpdate(index) }
                            onClicked: {
                                if (releases.rtlEnabled(index) || releases.rtlPending(index)) {
                                    confirmUpdate.productIndex = index
                                    confirmUpdate.open()
                                } else if (releases.externalInstall(index).length) {
                                    confirmMigration.productIndex = index
                                    confirmMigration.open()
                                } else {
                                    releases.install(index)
                                }
                            }
                        }
                            Button {
                                text: card.expanded ? label("Hide details", "بستن جزئیات") : label("Details", "جزئیات")
                                flat: true
                                onClicked: card.expanded = !card.expanded
                            }
                        }
                        Column {
                            visible: card.expanded
                            width: parent.width
                            spacing: 10
                            Label {
                                visible: card.updateAvailable
                                text: card.modelData.split("\n")[0]
                            }
                            TextEdit {
                                width: parent.width
                                LayoutMirroring.enabled: false
                                horizontalAlignment: Text.AlignLeft
                                text: card.modelData.split("\n").slice(1).join("\n")
                                visible: text.length > 0
                                textFormat: TextEdit.PlainText
                                wrapMode: TextEdit.WrapAnywhere
                                readOnly: true
                                selectByMouse: true
                                color: systemColors.text
                            }
                    Column {
                        property string folder: { releases.results; return releases.installFolder(index) }
                        width: parent.width
                        visible: folder.length > 0
                        Label { text: label("Installation folder", "پوشهٔ نصب") }
                        TextEdit {
                            width: parent.width
                            LayoutMirroring.enabled: false
                            horizontalAlignment: Text.AlignLeft
                            text: parent.folder
                            textFormat: TextEdit.PlainText
                            wrapMode: TextEdit.WrapAnywhere
                            readOnly: true
                            selectByMouse: true
                            color: systemColors.text
                        }
                        Button {
                            text: label("Open installation folder", "بازکردن پوشهٔ نصب")
                            onClicked: releases.openInstallFolder(index)
                        }
                    }

                            Flow {
                                visible: card.isInstalled
                                width: parent.width
                                spacing: 8
                        Button {
                            text: label("Restore previous version", "بازگردانی نسخهٔ قبلی")
                            enabled: { releases.results; return !releases.busy && releases.canRollback(index) }
                            onClicked: releases.rollback(index)
                        }
                        Button {
                            text: label("Remove", "حذف")
                            enabled: { releases.results; return !releases.busy && releases.installed(index) }
                            onClicked: {
                                confirmRemoval.productIndex = index
                                confirmRemoval.open()
                            }
                        }
                            }
                    Button {
                        text: {
                            releases.results
                            return releases.rtlEnabled(index)
                                ? label("Remove RTL", "حذف راست‌چین")
                                : releases.rtlPending(index)
                                    ? label("Re-enable RTL", "فعال‌کردن دوبارهٔ راست‌چین")
                                    : label("Enable RTL", "فعال‌کردن راست‌چین")
                        }
                        enabled: {
                            releases.results
                            return !releases.busy && releases.installed(index)
                                && (releases.rtlEnabled(index) || releases.rtlVersionKnown(index))
                        }
                        onClicked: releases.toggleRtl(index)
                    }
                    Label {
                        visible: { releases.results; return releases.rtlPending(index) }
                        text: {
                            releases.results
                            return releases.rtlVersionKnown(index)
                                ? label("RTL was enabled before the update. Enable it again if you want to keep it.",
                                        "راست‌چین پیش از به‌روزرسانی فعال بود. اگر هنوز آن را می‌خواهید، دوباره فعالش کنید.")
                                : label("RTL preference saved. Compatibility with this version is unverified.",
                                        "انتخاب راست‌چین ذخیره شد. سازگاری با این نسخه هنوز تأیید نشده است.")
                        }
                        width: parent.width
                        wrapMode: Text.Wrap
                    }
                        }
                    }
                }
            }

            Label {
                width: parent.width
                wrapMode: Text.Wrap
                text: releases.installation
            }

            ProgressBar {
                width: parent.width
                visible: releases.installing
                indeterminate: releases.progress < 0
                to: 100
                value: Math.max(0, releases.progress)
            }
            Flow {
                width: parent.width
                spacing: 8
                Button {
                    text: label("Save support report", "ذخیرهٔ گزارش پشتیبانی")
                    onClicked: reportFile.open()
                }
                Button {
                    text: label("Sign-in help", "راهنمای ورود")
                    onClicked: Qt.openUrlExternally("https://antigravity.google/docs/faq/")
                }
                Button {
                    visible: releases.managerUpdate.toString().length > 0
                    text: label("Download manager update", "دریافت به‌روزرسانی مدیر")
                    onClicked: releases.openManagerUpdate()
                }
            }
            Label { id: reportStatus; visible: text.length > 0 }
        }
    }

    SystemPalette { id: systemColors }
    Component.onCompleted: {
        releases.check()
        releases.checkManagerUpdate()
    }
}
