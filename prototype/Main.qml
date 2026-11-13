import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    function label(english, persian) { return releases.persian ? persian : english }
    LayoutMirroring.enabled: releases.persian
    LayoutMirroring.childrenInherit: true
    width: 620
    height: 650
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

            ComboBox {
                model: ["English", "فارسی"]
                currentIndex: releases.persian ? 1 : 0
                enabled: !releases.busy
                Accessible.name: label("Interface language", "زبان رابط")
                onActivated: releases.persian = currentIndex === 1
            }

            Button {
                text: label("Check Linux x64 releases", "بررسی نسخه‌های لینوکس x64")
                enabled: !releases.busy
                onClicked: releases.check()
            }

            Repeater {
                model: releases.results
                delegate: Column {
                    required property string modelData
                    required property int index
                    width: parent.width

                    TextEdit {
                        width: parent.width
                        LayoutMirroring.enabled: false
                        horizontalAlignment: Text.AlignLeft
                        text: modelData
                        textFormat: TextEdit.PlainText
                        wrapMode: TextEdit.WrapAnywhere
                        readOnly: true
                        selectByMouse: true
                        color: systemColors.text
                    }

                    Label {
                        property string version: { releases.results; return releases.localVersion(index) }
                        text: version.length ? label("Installed version: ", "نسخهٔ نصب‌شده: ") + "\u2066" + version + "\u2069" : label("Installed version unknown", "نسخهٔ نصب‌شده نامشخص است")
                        visible: { releases.results; return releases.installed(index) }
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

                    Row {
                        spacing: 8
                        Button {
                            text: label("Launch", "اجرا")
                            visible: { releases.results; return releases.installed(index) }
                            enabled: !releases.busy
                            onClicked: releases.launch(index)
                        }
                        Button {
                            text: { releases.results; return releases.installed(index) ? label("Update", "به‌روزرسانی") : label("Install", "نصب") }
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
                    Label {
                        property string detected: { releases.results; return releases.externalInstall(index) }
                        visible: detected.length > 0
                        width: parent.width
                        wrapMode: Text.WrapAnywhere
                        text: label("Existing installation found: ", "نصب قبلی پیدا شد: ") + "\u2066" + detected + "\u2069"
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
            Button {
                text: label("Save support report", "ذخیرهٔ گزارش پشتیبانی")
                onClicked: reportFile.open()
            }
            Label { id: reportStatus }
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
    }

    SystemPalette { id: systemColors }
    Component.onCompleted: {
        releases.check()
        releases.checkManagerUpdate()
    }
}
