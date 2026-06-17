\# Camera Analysis for Autonomous Systems – Scientific Paper Data Repository



\## Overview



This repository contains all data, analysis results, images, and supporting documents used for the \*\*Camera Analysis\*\* section of the scientific paper on \*\*Autonomous Systems\*\*.



The repository is organized to provide easy access to processed results, visualizations, YOLO outputs, and the original image datasets used during the analysis.



\---



\## Repository Structure



```

├── Documents/

├── YOLO Values/

└── Images/

```



\### 1. Documents



This folder contains the reports and graphical outputs generated during the analysis.



\#### Files



\* \*\*Camera\_analysis\_v2.docx\*\*



&#x20; \* Complete camera analysis report.

&#x20; \* Includes graphs, grayscale image results, and corresponding YOLO output values.



\* \*\*Graphs.docx\*\*



&#x20; \* Contains plots showing the relationship between \*\*pixel concentration\*\* and \*\*grayscale intensity\*\*.



\* \*\*One Page Report.docx\*\*



&#x20; \* A condensed summary of the complete analysis presented in a single-page format.



\---



\### 2. YOLO Values



This folder contains the object detection outputs generated using YOLO.



\#### Files



\* \*\*Clear.xlsx\*\*



&#x20; \* YOLO values obtained from clear images.



\* \*\*Noisy.xlsx\*\*



&#x20; \* YOLO values obtained from noisy images.



Both spreadsheets contain YOLO results for all images across all recorded timestamps.



\---



\### 3. Images



This folder contains the image dataset used in the analysis.



The structure is organized according to image quality levels and experimental distances:



```

Images/

├── Clear/

│   ├── High/

│   │   ├── d10/

│   │   ├── d15/

│   │   └── d20/

│   ├── Mid/

│   │   ├── d10/

│   │   ├── d15/

│   │   └── d20/

│   └── Low/

│       ├── d10/

│       ├── d15/

│       └── d20/

└── Noisy/

&#x20;   ├── High/

&#x20;   ├── Mid/

&#x20;   └── Low/

```



Where:



\* \*\*High\*\*, \*\*Mid\*\*, and \*\*Low\*\* represent different image quality or concentration levels.

\* \*\*d10\*\*, \*\*d15\*\*, and \*\*d20\*\* represent the different distance settings used during data collection.



\---



\## Purpose



This repository serves as a supplementary data source for the scientific paper on \*\*Autonomous Systems\*\*, providing transparency and reproducibility for the camera analysis results.



\---



\## Contents Summary



| Folder        | Description                                             |

| ------------- | ------------------------------------------------------- |

| `Documents`   | Reports, graphs, and analysis summaries                 |

| `YOLO Values` | YOLO output data for clear and noisy images             |

| `Images`      | Original image datasets organized by level and distance |



\---



\## Author



Repository created for research and scientific paper development in \*\*Autonomous Systems\*\*.



