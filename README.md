---

# Xscrape: An Image Scraper

Xscrape is a powerful tool designed to efficiently scrape images from websites. It allows users to specify their scraping criteria through a `.xapi` configuration file, making it versatile for various scraping needs.

## Features

- **Custom Configuration**: Utilize `.xapi` files to define scraping rules and targets, allowing for tailored scraping experiences.
- **Parallel Execution**: Xscrape supports parallel execution through PolyRun, enabling multiple instances of Xscrape to run simultaneously for increased efficiency.
- **User-Friendly**: Simple prompts guide users through the execution process, making it accessible for both beginners and experienced users.

## Getting Started

### Prerequisites

All necessary dependencies are included within the `vendor` directory of the Xscrape repository.

### Installation

Xscrape is a standalone application and does not require installation as a library. Simply clone the repository or download the source code to get started.

### Usage

1. **Run Xscrape**: Launch the Xscrape application. You will be prompted to select a `.xapi` configuration file.
   
   ```
   Please select a .xapi file:
   ```

2. **Create a `.xapi` File**: Follow the documentation in `XAPI-CONFIG-DOCS` to create a `.xapi` file that specifies the websites and images you wish to scrape.

3. **Use PolyRun for Parallel Execution**: To run multiple instances of Xscrape concurrently, use the PolyRun feature. This allows for faster scraping by distributing the workload across several instances.

   ```
   To use PolyRun, simply execute the PolyRun command followed by the .xapi files you wish to use:
   PolyRun example1.xapi example2.xapi
   ```

---

## Creating `.xapi` Configuration Files

### General Rules

- Each keyword must be on a separate line, and every line must contain only one keyword.
- Blank lines between keywords are permitted.
- Whitespaces (spaces, tabs, etc.) are generally ignored, except within `' \; '` and `' " '` characters.
- Comments can follow keywords (except after `limit` & `cqfname`) and will be ignored.

### Keywords

1. **`domain`**: Specifies the main website domain (e.g., `www.github.com` or `github.com`).
   - Syntax: `domain="<website>"`
   - Rules: Must be specified once.

2. **`target`**: Defines the sub-target path (e.g., `/subpage/me`).
   - Syntax: `target="<target>"`
   - Rules: Must be specified once.

3. **`bounds`**: A regex statement to search and navigate web pages recursively.
   - Syntax: `bounds={ \;<regex statement>\; \; <regex group for new link>\; <optional new bounds>{...} }`
   - Rules: Must be specified. Can be recursive.

4. **`incr`**: Specifies how to increment a part of the `target`.
   - Syntax: `incr=[<position>,<length>,<increment amount>]`
   - Rules: Optional. Can be specified multiple times.

5. **`limit`**: Sets the maximum number of increments.
   - Syntax: `limit=<number>`
   - Rules: Optional. Can only be specified once. Must be greater than 1.

6. **`append`**: Appends a runtime string to the `target`.
   - Syntax: `append=["<constant string>",<position/-1>,"<user prompt>"]`
   - Rules: Optional. `-1` appends at the end.

7. **`cqfname`**: Enables consecutive file naming.
   - Syntax: `cqfname=<true/false>`
   - Rules: Optional. Can only be specified once.

### Example

An example `.xapi` file to scrape images from a specific section of a website might look like this:

```
domain="www.example.com"
target="/images/gallery?page=1"
bounds={ \;src="(.+?\.jpg)"\; \; 1 \;}
incr=[22,1,1]
limit=10
cqfname=true
```

This configuration will scrape `.jpg` images from the gallery section of `www.example.com`, incrementing a part of the target URL to navigate through pages.

---

## License

Xscrape is made available under the MIT License, offering wide-ranging freedom for personal, educational, and commercial use, provided that the original copyright and license notice are included with any substantial portion of the software.

For the full license text, please refer to the [LICENSE](LICENSE) file in the repository.

---