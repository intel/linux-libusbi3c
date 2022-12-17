
# How To Contribute

First of all, thanks for taking the time to contribute!

**USB I3C\* Library for Linux\* OS** is open for code contributions. Nevertheless, if you are planning to propose a new feature, it's suggested that you file an issue first so we can discuss the feature idea and the proposed implementation. This way we can avoid future conflicts with other planned features.

When your code is ready, open a pull request to have your patches reviewed. Patches are only going to be merged after being reviewed and approved by at least 2 maintainers (with some exceptions for trivial patches), all functional tests pass and enough tests are added to test the proposed functionality or bug fix. Make sure your git user.name and git user.email are set correctly.

You should also add tests to the feature or bug fix you are proposing. That's how we ensure we won't have software regressions or we won't break any functionality in the future. We are currently enforcing a minimum of 95% of unit test coverage for our code base, so if your patch causes that coverage percentage to fall below that threshold it will be rejected.

## Code Style

**USB I3C\* Library for Linux\* OS** code style is defined on .clang-format and checked with "make compliant". Make sure you are complying with the code style before creating a pull request.

### Comments

Comments are not only used to explain tricky parts of the code, but also used to generate the library documentation. Every exported (non static) function should be documented, so when adding new functions, make sure they include documentation. The same goes for function modifications. We use Doxygen to generate our API documentation, so use a Doxygen supported standard when documenting functions and structures.

The following guidelines apply when documenting functions:
 - Every exported function should belong to a group. You can find the existing groups in [usbi3c.dox](./src/usbi3c.dox). Feel free to add a new group if the new function being added doesn't quite fit any of the existing groups.
 - All function arguments should be documented, and it should be noted if the argument is an input, output, or input/output argument.
 - If function returns a value, describe what is returned

Use /* */ for multiline or single line header comments. It's ok to use // for comments in the code.

Some examples of good comments:
```c
/**
 * @ingroup usbi3c_target_device
 * @brief Gets the devices in the target device table.
 *
 * These devices represent the I3C/I2C devices in the I3C bus.
 * The target device table can be freed using usbi3c_free_target_device_table()
 * when no longer needed.
 *
 * @note: This function is applicable when the usbi3c_dev device is the Active I3C Controller.
 *
 * @param[in] usbi3c_dev the usbi3c device (I3C controller)
 * @param[out] devices the list of devices in the table
 * @return the number of devices in the table if successful, or -1 otherwise
 */
int usbi3c_get_target_device_table(struct usbi3c_device *usbi3c_dev, struct usbi3c_target_device ***devices)

...
/* Check if the command was a success */
if (success) {
    return NO_ERRORS; // No errors reported
}
...
```

### Function Names

Always use meaningful function names, specially non-static functions. Avoid plurals and use imperative verbs. Add a prefix of the component/module to what this function refers to (usually the name of the header file). For example, usb_set_interrupt_context() is a better name than set_usb_interrupt_context() or set_interrupt_context_for_usb().

### Line Length

Avoid extremely long lines. Don't use lines longer than 80~100 characters.

### Git Commit Messages

 - Use the present tense ("Add feature" not "Added feature")
 - Use the imperative mood ("Fix issue with..." not "Fixes issue with...")
 - Limit the first line to 72 characters or less
 - Reference issues and pull requests liberally after the first line

