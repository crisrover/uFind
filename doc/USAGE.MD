# ufind

## Usage 
    ufind [path][file pattern ...][/option ...][-option ...][--longoption ...]
### Switches
     /r -r --recursive
         Search recursively in the given path.

     /f -f --files
         Search for files only excluding any directories that match the pattern.

     /d -d --dirs
         Search for directories only excluding any files that match the pattern.

     /c -c --match-case
         Case sensitive search.

     /a -a --additionalPath <additional path>
         Additional search path.

     /p -p --path
         Match the pattern against full path not only directory or file name.
         
    /i -i --info
         At the end of the search, statistics info will be displayed.

     /s -s --silent
         Silent mode, the only information that will be displayed is the search
         status, and at the end the minimal search result.

     /l -l --log
         Log mode - ONLY the files and directories found will be displayed, no
         additional info, use this when the output is redirected to a file.

     /h -h --help
         Display help.

     /?
         Quick help.

### Default settings
The simplest usage form of ufind is: 
* ufind c:

If no file pattern is specified, this will enable search for everything.
Also, by default, case sensitive search is disabled.
### Wildcard usage
ufind interprets * wildcard as [anything]
* \*   means that ufind will search for [anything]
* \*.* means that ufind will search for [anything].[anything] - notice the mandatory dot

## Examples

    ufind c:
Searches everything in root of drive c

    ufind c: \*nvidia\*nvdrs*.* -r -p
Output:  c:\Windows\System32\drivers\NVIDIA Corporation\Drs\nvdrsdb.bin
    NOTE: if using ufind with /p option, please note the * at the start of 
          the search pattern, if it's missing, ufind will search for something 
          starting with nvidia