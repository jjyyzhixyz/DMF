## DMF_Template

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Summary

-----------------------------------------------------------------------------------------------------------------------------------

A summary of the Module is documented here. More details are in the "[Module Remarks]" section below.

Template Module that makes it easy to copy entry point signatures and see the code that must be customized for every Module. This
document is an extension of that allows new documents to be made for new Modules.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Configuration

-----------------------------------------------------------------------------------------------------------------------------------
##### DMF_CONFIG_Template
````
typedef struct
{
  // TEMPLATE: Add context specifically used to open this DMF Module.
  //      (Remove TemplateDummy).
  //
  ULONG Dummy;
} DMF_CONFIG_Template;
````

##### Remarks

The Module's CONFIG structure is documented here.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Enumeration Types

-----------------------------------------------------------------------------------------------------------------------------------

##### Remarks

This section lists all the Enumeration Types specific to this Module that are accessible to Clients.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Callbacks

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Methods

-----------------------------------------------------------------------------------------------------------------------------------

Each of the Module's Methods is located here.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module IOCTLs

* None

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Remarks

* Detailed explanation about using the Module that Clients need to consider.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Children

* List of any Child Modules instantiated by this Module.

-----------------------------------------------------------------------------------------------------------------------------------

#### Module Implementation Details

-----------------------------------------------------------------------------------------------------------------------------------

#### Examples

* Examples of where the Module is used.
* ClientDriver
* Module

-----------------------------------------------------------------------------------------------------------------------------------

#### To Do

-----------------------------------------------------------------------------------------------------------------------------------

* List possible future work for this Module.

-----------------------------------------------------------------------------------------------------------------------------------
#### Module Category

-----------------------------------------------------------------------------------------------------------------------------------

Template

-----------------------------------------------------------------------------------------------------------------------------------
