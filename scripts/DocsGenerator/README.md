```py
def Initialize( logger: Logger, file: tuple[str, str, str, str], content_blocks: list[str] ) -> bool | str:

    '''
        Generate ConfigurationVariables table

        logger: Builder GenerateDocs's logger

        file:
            [0] Absolute path
            [1] relative path
            [2] filename
            [3] file extension

        content_blocks: List of block of content since the definition of the script til the end.

        return: True for suscess. string for failed and print the string as error
    '''
```
