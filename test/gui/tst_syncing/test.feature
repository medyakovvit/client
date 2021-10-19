Feature: Syncing files

    As a user
    I want to be able to sync my local folders to to my owncloud server
    so that I dont have to upload and download files manually

    Background:
        Given user "Alice" has been created on the server with default attributes and without skeleton files

    @smokeTest
    Scenario: Syncing a file to the server
        Given user "Alice" has set up a client with default settings
        When the user creates a file "lorem-for-upload.txt" with the following content on the file system
            """
            test content
            """
        And the user waits for file "lorem-for-upload.txt" to be synced
        Then as "Alice" the file "lorem-for-upload.txt" on the server should have the content "test content"

    Scenario: Syncing a file from the server
        Given user "Alice" has set up a client with default settings
        And user "Alice" has uploaded file on the server with content "test content" to "uploaded-lorem.txt"
        When the user waits for file "uploaded-lorem.txt" to be synced
        Then the file "uploaded-lorem.txt" should exist on the file system with the following content
            """
            test content
            """

    Scenario: Syncing a file from the server and creating a conflict
        Given user "Alice" has uploaded file on the server with content "server content" to "/conflict.txt"
        And user "Alice" has set up a client with default settings
        And the user has waited for file "conflict.txt" to be synced
        And the user has paused the file sync
        And the user has changed the content of local file "conflict.txt" to:
            """
            client content
            """
        And user "Alice" has uploaded file on the server with content "changed server content" to "/conflict.txt"
        When the user resumes the file sync on the client
        And the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        # Then a conflict warning should be shown for 1 files
        Then the table of conflict warnings should include file "conflict.txt"
        And the file "conflict.txt" should exist on the file system with the following content
            """
            changed server content
            """
        And a conflict file for "conflict.txt" should exist on the file system with the following content
            """
            client content
            """

    Scenario Outline: Syncing a folder to the server
        Given user "Alice" has set up a client with default settings
        When the user creates a folder "<foldername>"
        And the user waits for the folder "<foldername>" to sync
        Then as "Alice" folder "<foldername>" should exist on the server
        Examples:
            | foldername                                                             |
            | myFolder                                                               |
            | really long folder name with some spaces and special char such as $%ñ& |


    Scenario: Many subfolders can be synced
        Given user "Alice" has set up a client with default settings
        And the user has created a folder "parent"
        When the user creates a folder "parent/subfolderEmpty1"
        And the user creates a folder "parent/subfolderEmpty2"
        And the user creates a folder "parent/subfolderEmpty3"
        And the user creates a folder "parent/subfolderEmpty4"
        And the user creates a folder "parent/subfolderEmpty5"
        And the user creates a folder "parent/subfolder1"
        And the user creates a folder "parent/subfolder2"
        And the user creates a folder "parent/subfolder3"
        And the user creates a folder "parent/subfolder4"
        And the user creates a folder "parent/subfolder5"
        And the user creates a file "parent/subfolder1/test.txt" with the following content on the file system
            """
            test content
            """
        And the user creates a file "parent/subfolder2/test.txt" with the following content on the file system
            """
            test content
            """
        And the user creates a file "parent/subfolder3/test.txt" with the following content on the file system
            """
            test content
            """
        And the user creates a file "parent/subfolder4/test.txt" with the following content on the file system
            """
            test content
            """
        And the user creates a file "parent/subfolder5/test.txt" with the following content on the file system
            """
            test content
            """
        And the user waits for the folder "parent" to sync
        Then as "Alice" folder "parent/subfolderEmpty1" should exist on the server
        And as "Alice" folder "parent/subfolderEmpty2" should exist on the server
        And as "Alice" folder "parent/subfolderEmpty3" should exist on the server
        And as "Alice" folder "parent/subfolderEmpty4" should exist on the server
        And as "Alice" folder "parent/subfolderEmpty5" should exist on the server
        And as "Alice" folder "parent/subfolder1" should exist on the server
        And as "Alice" folder "parent/subfolder2" should exist on the server
        And as "Alice" folder "parent/subfolder3" should exist on the server
        And as "Alice" folder "parent/subfolder4" should exist on the server
        And as "Alice" folder "parent/subfolder5" should exist on the server


    Scenario: Both original and copied folders can be synced
        Given user "Alice" has set up a client with default settings
        And the user has created a folder "original"
        And the user has created a file "original/test.txt" with the following content on the file system
            """
            test content
            """
        When the user copies the folder "original" to "copied"
        And the user waits for the folder "copied" to sync
        Then as "Alice" folder "original" should exist on the server
        And as "Alice" folder "copied" should exist on the server


    Scenario: Verify that you can create a subfolder with long name
        Given user "Alice" has set up a client with default settings
        And the user has created a folder "Folder1"
        When the user creates a folder "Folder1/really long folder name with some spaces and special char such as $%ñ&"
        And the user creates a file "Folder1/really long folder name with some spaces and special char such as $%ñ&/test.txt" with the following content on the file system
            """
            test content
            """
        And the user waits for the folder "Folder1" to sync
        Then as "Alice" folder "Folder1" should exist on the server
        And as "Alice" folder "Folder1/really long folder name with some spaces and special char such as $%ñ&" should exist on the server
        And the file "Folder1/really long folder name with some spaces and special char such as $%ñ&/test.txt" should exist on the file system with the following content
            """
            test content
            """
        #And as "Alice" the file "Folder1/really long folder name with some spaces and special char such as $%ñ&/test.txt" on the server should have the content "test content"


    Scenario: Verify pre existing folders in local (Desktop client) are copied over to the server
        Given the user has created a folder "Folder1"
        And the user has created a folder "Folder1/subFolder1"
        And the user has created a folder "Folder1/subFolder1/subFolder2"
        And user "Alice" has set up a client with default settings
        When the user waits for the folder "Folder1" to sync
        Then as "Alice" folder "Folder1" should exist on the server
        And as "Alice" folder "Folder1/subFolder1" should exist on the server
        And as "Alice" folder "Folder1/subFolder1/subFolder2" should exist on the server


     Scenario: Filenames that are rejected by the server are reported
        Given user "Alice" has set up a client with default settings
        And the user has created a folder "Folder1"
        When the user creates a file "Folder1/a\\a.txt" with the following content on the file system
            """
            test content
            """
        And the user waits for the folder "Folder1" to sync
        Then as "Alice" folder "Folder1" should exist on the server
        When the user clicks on the activity tab
        And the user selects "Not Synced" tab in the activity
        # Then a conflict warning should be shown for 1 files
        Then the table of conflict warnings should include file "Folder1/a\\a.txt"
