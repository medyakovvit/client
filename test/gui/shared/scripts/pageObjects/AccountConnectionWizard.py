import names
import squish
from helpers.SetupClientHelper import getClientDetails
import test


class AccountConnectionWizard:
    SERVER_ADDRESS_BOX = names.leUrl_OCC_PostfixLineEdit
    NEXT_BUTTON = names.owncloudWizard_qt_passive_wizardbutton1_QPushButton
    USERNAME_BOX = names.leUsername_QLineEdit
    PASSWORD_BOX = names.lePassword_QLineEdit
    SELECT_LOCAL_FOLDER = names.pbSelectLocalFolder_QPushButton
    DIRECTORY_NAME_BOX = names.fileNameEdit_QLineEdit
    CHOOSE_BUTTON = names.qFileDialog_Choose_QPushButton
    FINISH_BUTTON = {
        "name": "qt_wizard_finish",
        "type": "QPushButton",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }
    ERROR_OK_BUTTON = {
        "text": "OK",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.error_QMessageBox,
    }
    ERROR_LABEL = {
        "name": "errorLabel",
        "type": "QLabel",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }
    CHOOSE_WHAT_TO_SYNC_BUTTON = {
        "name": "bSelectiveSync",
        "type": "QPushButton",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }

    CHOOSE_WHAT_TO_SYNC_CHECKBOX = names.choose_What_To_Synchronize_QTreeWidget
    CHOOSE_WHAT_TO_SYNC_OK_BUTTON = {
        "text": "OK",
        "type": "QPushButton",
        "unnamed": 1,
        "visible": 1,
        "window": names.choose_What_to_Sync_OCC_SelectiveSyncDialog,
    }
    MANUAL_SYNC_FOLDER_OPTION = {
        "name": "rManualFolder",
        "type": "QRadioButton",
        "visible": 1,
        "window": names.owncloudWizard_OCC_OwncloudWizard,
    }

    def __init__(self):
        pass

    def sanitizeFolderPath(self, folderPath):
        return folderPath.rstrip("/")

    def addServer(self, context):
        clientDetails = getClientDetails(context)
        squish.mouseClick(squish.waitForObject(self.SERVER_ADDRESS_BOX))
        squish.type(
            squish.waitForObject(self.SERVER_ADDRESS_BOX), clientDetails['server']
        )
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

    def addUserCreds(self, context):
        clientDetails = getClientDetails(context)
        squish.type(squish.waitForObject(self.USERNAME_BOX), clientDetails['user'])
        squish.type(squish.waitForObject(self.USERNAME_BOX), "<Tab>")
        squish.type(squish.waitForObject(self.PASSWORD_BOX), clientDetails['password'])
        squish.clickButton(squish.waitForObject(self.NEXT_BUTTON))

    def selectSyncFolder(self, context):
        clientDetails = getClientDetails(context)

        try:
            squish.clickButton(squish.waitForObject(self.ERROR_OK_BUTTON))
        except LookupError:
            pass
        squish.clickButton(squish.waitForObject(self.SELECT_LOCAL_FOLDER))
        squish.mouseClick(squish.waitForObject(self.DIRECTORY_NAME_BOX))
        squish.type(
            squish.waitForObject(self.DIRECTORY_NAME_BOX), clientDetails['localfolder']
        )
        squish.clickButton(squish.waitForObject(self.CHOOSE_BUTTON))
        test.compare(
            str(squish.waitForObjectExists(self.SELECT_LOCAL_FOLDER).text),
            self.sanitizeFolderPath(clientDetails['localfolder']),
        )

    def connectAccount(self):
        squish.clickButton(squish.waitForObject(self.FINISH_BUTTON))

    def addAccount(self, context):
        self.addServer(context)
        self.addUserCreds(context)
        self.selectSyncFolder(context)
        self.connectAccount()

    def openSyncDialog(self):
        squish.clickButton(squish.waitForObject(self.CHOOSE_WHAT_TO_SYNC_BUTTON))

    def selectManualSyncFolder(self):
        squish.clickButton(squish.waitForObject(self.MANUAL_SYNC_FOLDER_OPTION))

    def selectFoldersToSync(self, context):
        self.openSyncDialog()

        squish.mouseClick(
            squish.waitForObjectItem(self.CHOOSE_WHAT_TO_SYNC_CHECKBOX, "/"),
            11,
            11,
            squish.Qt.NoModifier,
            squish.Qt.LeftButton,
        )
        for row in context.table[1:]:
            squish.mouseClick(
                squish.waitForObjectItem(
                    self.CHOOSE_WHAT_TO_SYNC_CHECKBOX, "/." + row[0]
                ),
                11,
                11,
                squish.Qt.NoModifier,
                squish.Qt.LeftButton,
            )
        squish.clickButton(squish.waitForObject(self.CHOOSE_WHAT_TO_SYNC_OK_BUTTON))
